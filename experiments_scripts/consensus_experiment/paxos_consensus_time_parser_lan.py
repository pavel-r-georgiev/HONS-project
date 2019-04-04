
import csv
import glob
import itertools as it

def collect_data(log):
    l = []
    for line in log:
        l.extend([s for s in line.split() if s.replace('.','',1).isdigit()])

    return l


settings = ['3_nodes_1_failing', '4_nodes_1_failing', '4_nodes_2_failing', '5_nodes_1_failing', '5_nodes_2_failing']
data = []
current_setting = []

for s in settings:
    current_data = []
    for filename in glob.glob('../../logs/consensus_validation/lan/{}/paxos*'.format(s)):
        with open(filename) as f:
            a = collect_data(f)
            current_data.extend(a)
    data.append(current_data)

zzip = it.zip_longest(*data)

with open("paxos_lan.csv", "w") as m:
    writer = csv.writer(m)
    for z in zzip:
        writer.writerow(z)