def parse(f):
    ln = 0
    branch = {}
    defer = []
    for line in f:
        ln += 1
        if ln <9:
            print(line, end='')
            continue
        if ln> 40:
            defer.append(line)
            continue
        if(len(line) < 48):
            continue
        key = line[:48]
        if key not in branch:
            branch[key] = []
        branch[key].append(line[48:-1])


    print("{:<48}".format("Type"), end='')
    print("{:>18}".format("Forward"), end='')
    print("{:>20}".format("Backward"), end='')
    print("{:>20}".format("Overall"))
    for key in branch:
        print(key , end='')
        if key.startswith("Total"):
            for line in branch[key]:
                # padd print with 18 width
                print("{:<20}".format(line), end='')
            print()
            continue
        for line in branch[key]:
            print(line, end='')
            print("  ", end='')
        print()



    for line in defer:
        print(line, end='')

# loop over all files starting with br2
import os
import sys
for file in os.listdir("./"):
    if file.startswith("br2"):
        print(file)
        # override stdout to file
        f = open(file,"r")
        outfile = file[4:-19]
        sys.stdout = open(outfile,"w")
        parse(f)
        sys.stdout = sys.__stdout__
        f.close()
        print()
# f = open("./br2.400.perlbench.diffmail.out.20231011141546","r").20231011141546