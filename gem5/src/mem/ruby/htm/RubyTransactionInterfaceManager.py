from m5.objects.ClockedObject import ClockedObject
from m5.params import *
from m5.proxy import *
from m5.objects.System import System


class TransactionInterfaceManager(ClockedObject):
    type = 'TransactionInterfaceManager'
    cxx_class = 'gem5::ruby::TransactionInterfaceManager'
    cxx_header = "mem/ruby/htm/TransactionInterfaceManager.hh"

    dcache = Param.RubyCache("")
    l1_cache = Param.RubyCache("")
    sequencer = Param.RubyTransactionalSequencer("")
    ruby_system = Param.RubySystem(Parent.any, "")
