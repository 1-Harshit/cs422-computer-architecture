def parse(f):
    ln = 0
    branch = {}
    defer = []
    for line in f:
        ln += 1
        if ln >24 and ln < 27:
            print(line, end='')
            continue
        continue
        if ln < 8 :
            # print(line, end='')
            continue
        if ln> 40:
            # defer.append(line)
            continue
        if(len(line) < 48):
            continue
        key = line[:48]
        if key not in branch:
            branch[key] = []
        branch[key].append(line[48:-1])
    return

    print("{:<48}".format("Type"), end='')
    print("{:>18}".format("Forward"), end='')
    print("{:>20}".format("Backward"), end='')
    print("{:>20}".format("Overall"))
    for key in branch:
        if key.startswith("Total"):
            # for line in branch[key]:
            #     # padd print with 18 width
            #     print("{:<20}".format(line), end='')
            # print()
            continue
        if key.startswith(("G. ", "C. ", "D. ", "E. ")):
            print(key , end='') 
            for line in branch[key]:
                print(line, end='')
                print("  ", end='')
            print()



    # for line in defer:
    #     print(line, end='')

# loop over all files starting with br2
import os
for file in os.listdir("./"):
    if file.startswith("4"):
        print(file)
        # override stdout to file
        f = open(file,"r")
        parse(f)
        f.close()
        print()