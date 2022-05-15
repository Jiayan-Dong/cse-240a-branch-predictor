import csv
import os
import sys
import re

"""
usage python3 run_all.py bp_name
"""

traces = ['fp_1', 'fp_2', 'int_1', 'int_2', 'mm_1', 'mm_2']

fileheader = ['benchmark', 'branches', 'incorrect', 'misprediction_rate']

results = []

overall_branches = 0
overall_incorrect = 0

for t in traces:
    xe = os.popen(
        "bunzip2 -kc ../traces/{}.bz2 | ./predictor --custom".format(t), 'r')
    output = xe.read()
    result = re.findall(r"\d+\.?\d*", output)
    results.append([t] + result)

    overall_branches += int(result[0])
    overall_incorrect += int(result[1])

results = results + [['overall', str(overall_branches), str(
    overall_incorrect), "{:.3f}".format(overall_incorrect * 100 / overall_branches)]]

bp_name = sys.argv[1]

with open('../results/{}.csv'.format(bp_name), 'w') as csvfile:
    cwriter = csv.writer(csvfile)
    cwriter.writerows([fileheader] + results)
