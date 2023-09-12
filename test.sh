#!/bin/bash

# declare first argument as prefix
prefix=$1

make TARGET=ia32 obj-ia32/HW1.so

cd spec_2006/400.perlbench/ && pin -t ../../obj-ia32/HW1.so -f 207 -o perlbench.diffmail.out -- ./perlbench_base.i386 -I./lib diffmail.pl 4 800 10 17 19 300 > perlbench.ref.diffmail.out 2> perlbench.ref.diffmail.err && cp perlbench.diffmail.out "../../runs/$prefix.400.perlbench.diffmail.out.$(date +%Y%m%d%H%M%S)" &

cd spec_2006/401.bzip2/ && pin -t ../../obj-ia32/HW1.so -f 301 -o bzip2.source.out -- ./bzip2_base.i386 input.source 280 > bzip2.ref.source.out 2> bzip2.ref.source.err && cp bzip2.source.out "../../runs/$prefix.401.bzip2.source.out.$(date +%Y%m%d%H%M%S)" &

cd spec_2006/403.gcc/ && pin -t ../../obj-ia32/HW1.so -f 107 -o gcc.cp-decl.out -- ./gcc_base.i386 cp-decl.i -o cp-decl.s > gcc.ref.cp-decl.out 2> gcc.ref.cp-decl.err && cp gcc.cp-decl.out "../../runs/$prefix.403.gcc.cp-decl.out.$(date +%Y%m%d%H%M%S)" &

cd spec_2006/429.mcf/ && pin -t ../../obj-ia32/HW1.so -f 377 -o mcf.out -- ./mcf_base.i386 inp.in > mcf.ref.out 2> mcf.ref.err && cp mcf.out "../../runs/$prefix.429.mcf.out.$(date +%Y%m%d%H%M%S)" &

cd spec_2006/450.soplex/ && pin -t ../../obj-ia32/HW1.so -f 364 -o soplex.ref.out -- ./soplex_base.i386 -m3500 ref.mps > soplex.ref.ref.out 2> soplex.ref.ref.err && cp soplex.ref.out "../../runs/$prefix.450.soplex.ref.out.$(date +%Y%m%d%H%M%S)" &

cd spec_2006/456.hmmer/ && pin -t ../../obj-ia32/HW1.so -f 264 -o hmmer.nph3.out -- ./hmmer_base.i386 nph3.hmm swiss41 > hmmer.ref.nph3.out 2> hmmer.ref.nph3.err && cp hmmer.nph3.out "../../runs/$prefix.456.hmmer.nph3.out.$(date +%Y%m%d%H%M%S)" &

cd spec_2006/471.omnetpp/ && pin -t ../../obj-ia32/HW1.so -f 43 -o omnetpp.out -- ./omnetpp_base.i386 omnetpp.ini > omnetpp.ref.log 2> omnetpp.ref.err && cp omnetpp.out "../../runs/$prefix.471.omnetpp.out.$(date +%Y%m%d%H%M%S)" &

cd spec_2006/483.xalancbmk/ && pin -t ../../obj-ia32/HW1.so -f 1331 -o xalancbmk.out -- ./xalancbmk_base.i386 -v t5.xml xalanc.xsl > xalancbmk.ref.out 2> xalancbmk.ref.err && cp xalancbmk.out "../../runs/$prefix.483.xalancbmk.out.$(date +%Y%m%d%H%M%S)" &
