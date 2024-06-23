#!/bin/bash

# 1. Run cyclictest
#../../../OSrealtimeTools/rt-tests/cyclictest -l100000000 -m -Sp90 -i200 -h400 -q >output
../../../OSrealtimeTools/rt-tests/cyclictest -l10000 -m -Sp90 -i200 -h400 -q > "../../../OSrealtimeTools/rt-tests/LatencyHistData/output"

# 2. Get maximum latency
max=`grep "Max Latencies" "../../../OSrealtimeTools/rt-tests/LatencyHistData/output" | tr " " "\n" | sort -n | tail -1 | sed s/^0*//`

# 3. Grep data lines, remove empty lines and create a common field separator
grep -v -e "^#" -e "^$" "../../../OSrealtimeTools/rt-tests/LatencyHistData/output" | tr " " "\t" > "../../../OSrealtimeTools/rt-tests/LatencyHistData/histogram" 

# 4. Set the number of cores, for example
cores=1

# 5. Create two-column data sets with latency classes and frequency values for each core, for example
for i in `seq 1 $cores`
do
  column=`expr $i + 1`
  cut -f1,$column "../../../OSrealtimeTools/rt-tests/LatencyHistData/histogram" >"../../../OSrealtimeTools/rt-tests/LatencyHistData/histogram"$i
done

# 6. Create plot command header
echo -n -e "set title \"Latency plot\"\n\
set terminal png\n\
set xlabel \"Latency (us), max $max us\"\n\
set logscale y\n\
set xrange [0:400]\n\
set yrange [0.8:*]\n\
set ylabel \"Number of latency samples\"\n\
set output \"plot.png\"\n\
plot " >"../../../OSrealtimeTools/rt-tests/LatencyHistData/plotcmd"

# 7. Append plot command data references
for i in `seq 1 $cores`
do
  if test $i != 1
  then
    echo -n ", " >>"../../../OSrealtimeTools/rt-tests/LatencyHistData/plotcmd"
  fi
  cpuno=`expr $i - 1`
  if test $cpuno -lt 10
  then
    title=" CPU$cpuno"
   else
    title="CPU$cpuno"
  fi
  echo -n "\"histogram$i\" using 1:2 title \"$title\" with histeps" >>"../../../OSrealtimeTools/rt-tests/LatencyHistData/plotcmd"
done

# 8. Execute plot command
#gnuplot -persist <plotcmd
# Source: https://www.osadl.org/uploads/media/mklatencyplot.bash
