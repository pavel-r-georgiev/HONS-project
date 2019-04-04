import csv
import glob
import itertools as it



def collect_data(log):
    l = []
    for line in log:
      if(len(line.split('|')) == 5):
        l.extend([s for s in line.split() if s.replace('.','',1).isdigit()])

    return l

settings = ['1000-1', '1000-10', '10000-1', '10000-10', 'static']
data = []
current_setting = []

for s in settings:
    current_data = []
    for filename in glob.glob('../../logs/adaptive_timeout/network_delay/drastic_delay/{}/fd*'.format(s)):
        with open(filename) as f:
            a = collect_data(f)
            current_data.extend(a)
    data.append(current_data)

zzip = it.zip_longest(*data)

with open("adaptive_timeout_mistake_dur_local_delay_drastic.csv", "w") as m:
    writer = csv.writer(m)
    for z in zzip:
        writer.writerow(z)