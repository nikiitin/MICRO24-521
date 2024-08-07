from m5.objects.ClockedObject import ClockedObject
from m5.params import *
from m5.proxy import *
from m5.objects.System import System
from m5.objects.HTM import *

class RubyHTM(HTM):
    type = 'RubyHTM'
    cxx_class = 'gem5::ruby::RubyHTM'
    cxx_header = "mem/ruby/htm/htm.hh"
