def dataFromFile(filename):
    file = open(filename, "r")
    start = -1.0
    end = -1.0
    throughput = {}
    for line in file:
        if (line[0] != '%'):
            data = line.split('\t')
            if start != -1:
                start = min(start, float(data[0]))
            else:
                start = float(data[0])
            if end != -1:
                end = max(end, float(data[1]))
            else:
                end = float(data[1])
            if data[3] in throughput: 
                throughput[data[3]] += int(data[7])
            else:
                throughput[data[3]] = int(data[7])
    for k in sorted(throughput):
        throughput[k] /= end-start
        throughput[k] *= 8
    file.close()
    return throughput
dl = dataFromFile("DlRlcStats.txt")
ul = dataFromFile("UlRlcStats.txt")
for k in dl:
    print("UE " + k + ":")
    print("\tDL: " + str(dl[k]) + " bits per second")
    print("\tUL: " + str(ul[k]) + " bits per second")
