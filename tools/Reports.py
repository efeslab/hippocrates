from argparse import ArgumentParser
from copy import deepcopy
from enum import Enum, auto
from pathlib import Path
from IPython import embed

import collections
import re

# https://pyyaml.org/wiki/PyYAMLDocumentation
import yaml
try:
    from yaml import CLoader as Loader, CDumper as Dumper
except ImportError:
    print(f'Could not import CLoader/CDumper, gonna be slow')
    exit(-1)
    from yaml import Loader, Dumper

class BugReportSource(Enum):
    GENERIC = auto()
    PMTEST = auto()

class TraceEvent(collections.abc.Mapping):
    '''
        Represents an event in the trace of events.
    '''

    class EventType(Enum):
        '''
            All the PM operations:
                - Store
                - Flush
                - Fence
            
            Some operations are compound:
                - MOVNT: store + flush
                - CLFLUSH: flush + LOCAL fence
        '''
        STORE = auto()
        FLUSH = auto()
        FENCE = auto()
        '''
            All the PM assertions.

                Correctness:
                    - Missing flush (universal persistency issue)
                    - Missing fence (universal ordering issue)
            
                Performance:
                    - Extraneous flush (universal persistency)
            
            There are some things which we cannot fix:

                Correctness:
                    - Semantic ordering (could cause logical side effects)
                
                Performance:
                    - Extraneous fence (could cause concurrent side effects)
        '''
        ASSERT_PERSISTED = auto()
        ASSERT_ORDERED = auto()
        REQUIRED_FLUSH = auto()

    def _validate(self):
        '''
            Ensures that the trace event has all the fields we want in it.
        '''
        assert hasattr(self, 'timestamp') and isinstance(self.timestamp, int)
        assert hasattr(self, 'event') and isinstance(self.event, self.EventType)
        assert hasattr(self, 'function') and isinstance(self.function, str)
        assert hasattr(self, 'file') and isinstance(self.file, str)
        assert hasattr(self, 'line') and isinstance(self.line, int)
        assert hasattr(self, 'is_bug') and isinstance(self.is_bug, bool)
        assert hasattr(self, 'stack') and isinstance(self.stack, list)
        for sf in self.stack:
            assert isinstance(sf, dict)
            assert 'function' in sf and isinstance(sf['function'], str)
            assert 'file' in sf and isinstance(sf['file'], str)
            assert 'line' in sf and isinstance(sf['line'], int)

        if self.event == self.EventType.STORE or \
                self.event == self.EventType.FLUSH or \
                self.event == self.EventType.ASSERT_PERSISTED or \
                self.event == self.EventType.REQUIRED_FLUSH:
            assert hasattr(self, 'address') and isinstance(self.address, int)
            assert hasattr(self, 'length') and isinstance(self.length, int)
        elif self.event == self.EventType.ASSERT_ORDERED:
            assert hasattr(self, 'address_a') and isinstance(self.address_a, int)
            assert hasattr(self, 'length_a') and isinstance(self.length_a, int)
            assert hasattr(self, 'address_b') and isinstance(self.address_b, int)
            assert hasattr(self, 'length_b') and isinstance(self.length_b, int)

    def __init__(self, original=None, **kwargs):
        if original is not None:
            self.__dict__.update(original)
        self.__dict__.update(
            {k: v for k, v in kwargs.items() if v is not None})

        if 'event' in self.__dict__:
            self.__dict__['event'] = self.EventType[self.__dict__['event']]
        
        self._validate()
    
    def __iter__(self):
        return iter(self.__dict__)
    
    def __getitem__(self, a):
        item = getattr(self, a)
        if isinstance(item, self.EventType):
            return item.name
        return item
    
    def __len__(self):
        return len(self.__dict__)


class BugReport:
    def __init__(self, output_file):
        assert isinstance(output_file, Path)
        self.output_file = output_file
        self.trace = []
        self.metadata = {}

    def set_source(self, src):
        assert(isinstance(src, BugReportSource))
        self.metadata['source'] = src.name

    def _validate_metadata(self):
        ''' Check that all the required metadata as been setup. '''
        assert ('source' in self.metadata), 'call set_source(BugReportSource)!'

    def _add_internal(self, **kwargs):
        self.trace += [kwargs]

    def add_trace_event(self, te: TraceEvent) -> None:
        self._add_internal(**te)

    @staticmethod
    def _is_bug(te: TraceEvent) -> bool: 
        bug_events = ['ASSERT_PERSISTED', 'ASSERT_ORDERED', 'REQUIRED_FLUSH']
        return te['event'] in bug_events

    @classmethod
    def _freeze(cls, data):
        '''
            Generate a frozen data type from this
        '''
        if isinstance(data, dict):
            return frozenset((key, cls._freeze(value)) for key, value in data.items())
        elif isinstance(data, list):
            return tuple(cls._freeze(value) for value in data)
        return data

    @classmethod
    def _get_bug_addresses(cls, bug):
        assert cls._is_bug(bug)
        
        if bug['event'] == 'ASSERT_ORDERED':
            a1 = (bug['address_a'], bug['address_a'] + bug['length_a'])
            a2 = (bug['address_b'], bug['address_b'] + bug['length_b'])
            return a1, a2
        else:
            addr = (bug['address'], bug['address'] + bug['length'])
            return addr, None

    def _opt_remove_bugs(self) -> list: 
        from intervaltree import IntervalTree, Interval
        is_flush = lambda x: x['event'] == 'FLUSH'
        is_store = lambda x: x['event'] == 'STORE'
        is_fence = lambda x: x['event'] == 'FENCE'
        get_range = lambda x: (x['address'], x['address'] + x['length'])
        '''
            All stores should now be related to some bug.

            Now, we want to remove bugs which have redundant fix locations.
            We do this in the fixer as well, but it costs a lot of extra
            effort.
        '''

        bugs = [x for x in self.trace if self._is_bug(x)]

        # First, we should create a global update thing
        '''
            - Make an interval tree for stores
            - Make an interval tree for flushes
            -
        '''
        store_tree = IntervalTree()
        flush_tree = IntervalTree()
        for te in self.trace:
            if is_store(te):
                addr = get_range(te)
                store_tree.addi(addr[0], addr[1], te)
            if is_flush(te):
                addr = get_range(te)
                ivts = store_tree.overlap(*addr)
                store_tree.discardi(*addr)
                for ivt in ivts:
                    flush_tree.add(ivt)
            if is_fence(te):
                flush_tree.clear()

        # Maps (type, location) -> bug
        fix_locs = {}
        new_trace = []
        for tidx, te in enumerate(self.trace):
            # print(f'opt: {tidx}/{len(self.trace)}')
            # print(f'loc: {len(fix_locs)}')
            if te not in bugs or te['event'] != 'ASSERT_PERSISTED':
                new_trace += [te]
                continue

            addr = (te['address'], te['address'] + te['length'])
            # Now, we need to reverse through the new trace and find which store
            # is the source of the bug.

            # What are we going to do here?
            def handle_overlaps(tree):
                ivts = tree.overlap(*addr)
                for ivt in ivts:
                    key = [ivt.data['event'], ivt.data['stack']]
                    # only frozen stuff is hashable
                    fkey = self._freeze(key)
                    if fkey not in fix_locs:
                        fix_locs[fkey] = te

            # First, find overlap in the store tree
            handle_overlaps(store_tree)

            # Second, find overlap in the flush tree
            handle_overlaps(flush_tree)
            
        # Now, add back the fix locations
        for _, bug in fix_locs.items():
            new_trace += [bug]

        get_timestamp = lambda x: x['timestamp']
        new_trace.sort(key=get_timestamp)

        return new_trace


    def _optimize(self):
        from intervaltree import IntervalTree, Interval
        ''' 
            Do a few things:
            1. Remove redundant bugs first.
            2. Remove everything that isn't related to a bug. 
        '''
        bugs = [x for x in self.trace if self._is_bug(x)]

        is_flush = lambda x: x['event'] == 'FLUSH'
        is_store = lambda x: x['event'] == 'STORE'
        is_fence = lambda x: x['event'] == 'FENCE'
        get_timestamp = lambda x: x['timestamp']

        # Step 1: Remove bugs from redundant locations
        new_trace = self._opt_remove_bugs()

        print(f'(Step 1) Optimized from {len(self.trace)} trace events to {len(new_trace)} trace events.')
        self.trace = new_trace

        # Step 2: Remove redundancies.
        '''
        - This includes things like redundant store locations
            - For each bug, get the address. Then, for the stores that match that
            address, remove them
        '''
        unique_bug_addrs = IntervalTree()
        new_trace = []
        for te in self.trace:
            if not self._is_bug(te):
                continue

            a1, a2 = self._get_bug_addresses(te)

            if a1 is not None:
                unique_bug_addrs.addi(a1[0], a1[1], True)
            
            if a2 is not None:
                unique_bug_addrs.addi(a2[0], a2[1], True)

        # Now we have the bug addresses. We now remove stores unrelated to those.
        # We will remove the ranges as we reverse through the list.
        new_trace = []
        for te in reversed(self.trace):
            if te['event'] != 'STORE':
                new_trace += [te]
                continue

            addr = (te['address'], te['address'] + te['length'])
            
            if unique_bug_addrs.overlap(*addr):
                new_trace += [te]
                # unique_bug_addrs.remove_overlap(*addr)

        new_trace.reverse()

        print(f'(Step 2) Optimized from {len(self.trace)} trace events to {len(new_trace)} trace events.')
        self.trace = new_trace


        '''
        Now, we want to remove all repeated stores between flushes. Essentially,
        if store X, store X, ... flush X, we only want the most recent store X.
        '''

        in_flight = IntervalTree()
        new_trace = []
        for te in self.trace:
            if not is_flush(te) and not is_store(te):
                new_trace += [te]
                continue

            addr = (te['address'], te['address'] + te['length'])

            if is_flush(te):
                # Get all the stores in the range, remove them, and add them to the
                # new trace
                tes = in_flight[addr[0]:addr[1]]
                if tes:
                    # Make them trace events again
                    new_trace += [x.data for x in tes]
                    in_flight.remove_overlap(addr[0], addr[1])
            
            if is_store(te):
                # Add to the range, overwriting anything before.
                in_flight.addi(addr[0], addr[1], te)
                # embed()

        # Now, I need to add all the things back that were never flushed
        new_trace += [x.data for x in in_flight[:]]

        # Sort the new_trace by timestamp
        # embed()
        new_trace.sort(key=get_timestamp)

        print(f'(Step X) Optimized from {len(self.trace)} trace events to {len(new_trace)} trace events.')
        self.trace = new_trace

        # Step: Remove redundant fences
        new_trace = []
        prev_te = None
        # for te in reversed(self.trace):
        for te in self.trace:
            if te in bugs:
                new_trace += [te]
                continue
            
            if te['event'] == 'FENCE':
                if prev_te is not None and prev_te['event'] == 'FENCE':
                    continue
        
            new_trace += [te]
            prev_te = te

        print(f'(Step 2) Optimized from {len(self.trace)} trace events to {len(new_trace)} trace events.')
        self.trace = new_trace
        
    
    def dump(self):
        self._validate_metadata()
        self._optimize()
        print(f'Prepare to dump.\n\tNum items: {len(self.trace)}')
        report = {'trace': self.trace, 'metadata': self.metadata}
        raw = yaml.dump(report, None, Dumper=Dumper)
        print('Prepare to write.')
        with self.output_file.open('w') as f:
            f.write(raw)
            # yaml.dump(report, f)
        print(f'Report written to {str(self.output_file)}')

    def __getitem__(self, a):
        return self.trace[a]

class TraceUtils:
    COLOR_RE = re.compile('\\033\[\d+m')

    @staticmethod
    def strip_color(word):
        ''' Remove the color ASCII sequence. '''
        removed = TraceUtils.COLOR_RE.sub('', word)
        return removed
