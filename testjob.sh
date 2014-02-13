#!/bin/sh
#PBS -N giraffe
#PBS -l nodes=1:ppn=12
#PBS -q optimist
#PBS -A freecycle
#PBS -lwalltime=00:05:00

cd $PBS_O_WORKDIR
./test_prefetcher.py

