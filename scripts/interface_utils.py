#!/usr/bin/python

import sys
import os
import subprocess
from parse_read import *
from parse_alignment import *

#filename = sys.argv[1]
#readarg = sys.argv[2]

def get_reads(filename, readlist):
    stream = subprocess.Popen(["DBshow", filename] + map(str,readlist),
                                      stdout=subprocess.PIPE,bufsize=1)
    reads = parse_read(stream.stdout) # generator
    return reads
  

def get_alignments(filename, readlist):
    stream = subprocess.Popen(["LAshow", filename,filename]+ map(str,readlist),
                                      stdout=subprocess.PIPE,bufsize=1)
    alignments = parse_alignment(stream.stdout) # generator
    return alignments

def get_all_reads(filename):
    stream = subprocess.Popen(["DBshow", filename],
                                      stdout=subprocess.PIPE,bufsize=1)
    reads = parse_read(stream.stdout) # generator
    return reads

def get_all_alignments(filename):
    stream = subprocess.Popen(["LAshow", filename, filename ],
                                      stdout=subprocess.PIPE,bufsize=1)
    alignments = parse_alignment(stream.stdout) # generator
    return alignments

def get_all_reads_in_alignment_with_one(filename,read):
    this_read = get_reads(filename,[read])
    alignments = list(get_alignments(filename,[read]))
    readlist = map(lambda x:x[2],alignments)
    print readlist
    other_reads = get_reads(filename,readlist)
    
    return [list(this_read), list(other_reads), alignments] # note that this is not a generator


# test   
#for item in get_reads('G',[1]):
#    print item
    
#for item in get_alignments('G',[1]):
#    print item

#for item in get_all_alignments('G'):
#    print item

#for item in get_all_reads_in_alignment_with_one('G',1):
#    print item

#for item in get_reads('G', [1,2,3]):
#    print item

