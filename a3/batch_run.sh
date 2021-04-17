#!/bin/sh

make
for mode in OPT FIFO CLOCK LRU RANDOM
do	mkdir -p out/$mode
	for num_frames in 1 2 3 4 5 10 20 50 75 100 200 500 1000
	do	echo $mode/$num_frames
		time ./frames huge_trace.in $num_frames $mode -verbose > out/$mode/$num_frames.out
	done
done