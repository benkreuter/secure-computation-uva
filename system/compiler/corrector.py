#!/usr/bin/python

import sys
import anydbm

inf = open(sys.argv[1], "r")
map_file = open(sys.argv[2], "r")
if "usedb" in sys.argv:
    newids = anydbm.open('cache.1','n')
    oldids = anydbm.open('cache.2','n')
    gates = anydbm.open('cache.3', 'n')
else:
    newids = dict()
    oldids = dict()
    gates = dict()
opt = True
if "noopt" in sys.argv:
    opt = False
curline = 0;

# Some gates or subcircuits will always evaluate to zero or one; these
# are the lines that will always have it, so that we can just remap
# those gates here.
zeroline = -1;
oneline = -1;

for line in map_file:
    tokens = line.split(" ")
    oldids[tokens[0]] = tokens[1].rstrip('\n')

for line in inf:
    tokens = line.split(" ")
    newids[tokens[0]] = str(curline)
    if tokens[1] == "input":
        sys.stdout.write(line)
        curline = curline + 1
    elif not opt:
        s = cnt = 0
        inputs = {}
        comment = ""
        for tok in tokens:
            if tok == "inputs":
                s = cnt+2
            if tok == "]" and tokens[1] == "output":
                comment = tokens[cnt+1]
            cnt = cnt+1

        if tokens[1] == "output":
            n = 4
        else:
            n=3
        for i in range(0,int(tokens[n])):
            if tokens[s+i] in newids:
                inputs[i] = newids[tokens[s+i]]
            else:
                inputs[i] = newids[oldids[tokens[s+i]]]

        sys.stdout.write(str(curline) + " ")
        for i in range(1,s):
            sys.stdout.write(tokens[i] + " ")
        for q in inputs:
            sys.stdout.write(str(inputs[q]) + " ")
        sys.stdout.write("]")
        if(n == 3):
            sys.stdout.write("\n")
        else:
            sys.stdout.write(" "  + comment)
        curline = curline + 1
    else:
        inputs = dict()
        cnt = 0
        s = 0
        comment = ""
        
        if (not tokens[1] == "output") and tokens[3] == "1" and tokens[6] == "0" and tokens[7] == "1":
            # Identity wire == discard this
            if tokens[11] in newids:
                newids[tokens[0]] = newids[tokens[11]]
            else:
                newids[tokens[0]] = newids[oldids[tokens[11]]]
        elif (not tokens[1] == "output") and tokens[3] == "1" and tokens[6] == "0" and tokens[7] == "0" and zeroline > 0:
            newids[tokens[0]] = str(zeroline)
        elif (not tokens[1] == "output") and tokens[3] == "1" and tokens[6] == "1" and tokens[7] == "1" and oneline > 0:
            newids[tokens[0]] = str(oneline)

        else:
            for tok in tokens:
                if tok == "inputs":
                    s = cnt+2
                if tok == "]" and tokens[1] == "output":
                    comment = tokens[cnt+1]
                cnt = cnt+1

            if tokens[1] == "output":
                n = 4
            else:
                n=3
            for i in range(0,int(tokens[n])):
                if tokens[s+i] in newids:
                    inputs[i] = newids[tokens[s+i]]
                else:
                    inputs[i] = newids[oldids[tokens[s+i]]]

            if (not tokens[1] == "output") and tokens[3] == "1" and tokens[6] == "0" and tokens[7] == "0":
                zeroline = curline
            if (not tokens[1] == "output") and tokens[3] == "1" and tokens[6] == "1" and tokens[7] == "1":
                oneline = curline

            if (not tokens[1] == "output") and tokens[3] == "1" and tokens[6] == "1" and tokens[7] == "0":
                if not oneline > 0:
                    sys.stdout.write(str(curline) + " gate arity 1 table [ 1 1 ] inputs [ 1 ]\n")
                    oneline = curline
                    curline = curline+1
                sys.stdout.write(str(curline) + " gate arity 2 table [ 0 1 1 0 ] inputs [ " + str(oneline) + " " + str(inputs[0]) + " ]\n")
                newids[tokens[0]] = str(curline)

            elif (not tokens[1] == "output") and tokens[3] == "2" and tokens[6] == "0" and tokens[7] == "1" and tokens[8] == "1" and tokens[9] == "0" and inputs[0] == inputs[1]:
                if zeroline == -1:
                    zeroline = curline
                    sys.stdout.write(str(curline) + " gate arity 1 table [ 0 0 ] inputs [ 1 ]\n")
                    newids[tokens[0]] = str(curline)
                else:
                    newids[tokens[0]] = str(zeroline)
            elif (not tokens[1] == "output") and tokens[3] == "2" and tokens[6] == "1" and tokens[7] == "0" and tokens[8] == "0" and tokens[9] == "1" and inputs[0] == inputs[1]:
                if oneline == -1:
                    oneline = curline
                    sys.stdout.write(str(curline) + " gate arity 1 table [ 1 1 ] inputs [ 1 ]\n")
                    newids[tokens[0]] = str(curline)
                else:
                    newids[tokens[0]] = str(oneline)
            else:
    # find input tokens
                if (not tokens[1] == "output") and (tokens[3] == "2" or tokens[3] == "3"):
                    truth_table = None
                    ins = None
                    if tokens[3] == "2":
                        truth_table = (tokens[6], tokens[7], tokens[8], tokens[9])
                        ins = (inputs[0],inputs[1])
                    elif tokens[3] == "3":
                        truth_table = (tokens[6], tokens[7], tokens[8], tokens[9],
                                       tokens[10], tokens[11], tokens[12], tokens[13])
                        ins = (inputs[0], inputs[1], inputs[2])

                    ins = tuple(sorted(ins))

                    if gates.has_key(str((truth_table,ins))): #str((truth_table,ins)) in gates:
                        newids[tokens[0]] = gates[str((truth_table,ins))]
                    elif str(oneline) in ins:
                        # This gate has a constant wire.  Find it, rewrite as appropriate.
                        # This appears to actually increase the size of the circuit!?
                        arity = int(tokens[3])
                        if arity == 2:
                            old_truth_table = truth_table
                            idx = -1
                            if inputs[0] == str(oneline):
                                trut_table = (truth_table[1],truth_table[3])
                                idx = 0
                            else:
                                truth_table = (truth_table[2],truth_table[3])
                                idx = 1
                            if truth_table == ("0","1"):
                                if idx == 0: #inputs[0] == str(oneline):
                                    newids[tokens[0]] = inputs[1]
                                else:
                                    newids[tokens[0]] = inputs[0]
                                gates[str((old_truth_table,ins))] = newids[tokens[0]]
                            # elif truth_table == ("1","0"):
                            #     if not oneline > 0:
                            #         sys.stdout.write(str(curline) + " gate arity 1 table [ 1 1 ] inputs [ 1 ]\n")
                            #         oneline = curline
                            #         curline = curline+1
                            #     sys.stdout.write(str(curline) + " gate arity 2 table [ 0 1 1 0 ] inputs [ " + str(oneline) + " " + str(inputs[0]) + " ]\n")
                            #     newids[tokens[0]] = str(curline)
                            #     gates[str((old_truth_table,ins))] = newids[tokens[0]]
                            else:
                                gates[str((old_truth_table,ins))] = str(curline)
                                sys.stdout.write(str(curline) + " gate arity " + str(arity-1) + " table [ ")
                                for i in truth_table:
                                    sys.stdout.write(i + " ")
                                sys.stdout.write("] inputs [ ")
                                for q in inputs:
                                    if q != idx:
                                        sys.stdout.write(str(inputs[q]) + " ")
                                sys.stdout.write("]")
                                if(n == 3):
                                    sys.stdout.write("\n")
                                else:
                                    sys.stdout.write(" "  + comment)
                                curline = curline + 1
                        elif arity == 3:
                            old_truth_table = truth_table
                            idx = -1
                            if inputs[0] == str(oneline):
                                truth_table = (truth_table[1],truth_table[3],truth_table[5],truth_table[7])
                                idx = 0
                            elif inputs[1] == str(oneline):
                                truth_table = (truth_table[2],truth_table[3],truth_table[6],truth_table[7])
                                idx = 1
                            else:
                                truth_table = (truth_table[4],truth_table[5],truth_table[6],truth_table[7])
                                idx = 2
#                            sys.stderr.write(str(inputs) + " " + str(old_truth_table) + " " + str(truth_table) + "\n")
                            if truth_table == ("0", "1", "0", "1"):
                                if idx == 0:
                                    newids[tokens[0]] = inputs[1]
                                else:
                                    newids[tokens[0]] = inputs[0]
                                gates[str((old_truth_table,ins))] = newids[tokens[0]]
                            elif truth_table == ("0","0","1","1"):
                                if idx == 2:
                                    newids[tokens[0]] = inputs[1]
                                else:
                                    newids[tokens[0]] = inputs[2]
                                gates[str((old_truth_table,ins))] = newids[tokens[0]]
                            else:
                                gates[str((old_truth_table,ins))] = str(curline)
                                sys.stdout.write(str(curline) + " gate arity " + str(arity-1) + " table [ ")
                                for i in truth_table:
                                    sys.stdout.write(i + " ")
                                sys.stdout.write("] inputs [ ")
                                for q in inputs.keys():
                                    if q != idx:
                                        sys.stdout.write(str(inputs[q]) + " ")
                                sys.stdout.write("]")
                                if(n == 3):
                                    sys.stdout.write("\n")
                                else:
                                    sys.stdout.write(" "  + comment)
                                curline = curline + 1
                        else:
                            gates[str((truth_table,ins))] = str(curline)
                            sys.stdout.write(str(curline) + " ")
                            for i in range(1,s):
                                sys.stdout.write(tokens[i] + " ")
                            for q in inputs:
                                sys.stdout.write(str(inputs[q]) + " ")
                            sys.stdout.write("]")
                            if(n == 3):
                                sys.stdout.write("\n")
                            else:
                                sys.stdout.write(" "  + comment)
                            curline = curline + 1
                    elif str(zeroline) in ins:
                        arity = int(tokens[3])
                        if arity == 2:
                            old_truth_table = truth_table
                            idx = -1
                            if inputs[0] == str(zeroline):
                                truth_table = (truth_table[0],truth_table[2])
                                idx = 0
                            else:
                                truth_table = (truth_table[0],truth_table[1])
                                idx = 1
                            if truth_table == ("0","1"):
                                if idx == 0: #inputs[0] == str(zeroline):
                                    newids[tokens[0]] = inputs[1]
                                else:
                                    newids[tokens[0]] = inputs[0]
                                gates[str((old_truth_table,ins))] = newids[tokens[0]]
                            # elif truth_table == ("1","0"):
                            #     if not oneline > 0:
                            #         sys.stdout.write(str(curline) + " gate arity 1 table [ 1 1 ] inputs [ 1 ]\n")
                            #         oneline = curline
                            #         curline = curline+1
                            #     sys.stdout.write(str(curline) + " gate arity 2 table [ 0 1 1 0 ] inputs [ " + str(oneline) + " " + str(inputs[0]) + " ]\n")
                            #     newids[tokens[0]] = str(curline)
                            #     gates[str((old_truth_table,ins))] = newids[tokens[0]]
                            else:
                                gates[str((old_truth_table,ins))] = str(curline)
                                sys.stdout.write(str(curline) + " gate arity " + str(arity-1) + " table [ ")
                                for i in truth_table:
                                    sys.stdout.write(i + " ")
                                sys.stdout.write("] inputs [ ")
                                for q in inputs:
                                    if q != idx:
                                        sys.stdout.write(str(inputs[q]) + " ")
                                sys.stdout.write("]")
                                if(n == 3):
                                    sys.stdout.write("\n")
                                else:
                                    sys.stdout.write(" "  + comment)
                                curline = curline + 1
                        elif arity == 3:
                            old_truth_table = truth_table
                            idx = -1
                            if inputs[0] == str(zeroline):
                                truth_table = (truth_table[0],truth_table[2],truth_table[4],truth_table[6])
                                idx = 0
                            elif inputs[1] == str(zeroline):
                                truth_table = (truth_table[0],truth_table[1],truth_table[4],truth_table[5])
                                idx = 1
                            else:
                                truth_table = (truth_table[0],truth_table[1],truth_table[2],truth_table[3])
                                idx = 2
#                            sys.stderr.write(str(inputs) + " " + str(old_truth_table) + " " + str(truth_table) + "\n")
                            if truth_table == ("0", "1", "0", "1"):
                                if idx == 0:
                                    newids[tokens[0]] = inputs[1]
                                else:
                                    newids[tokens[0]] = inputs[0]
                                gates[str((old_truth_table,ins))] = newids[tokens[0]]
                            elif truth_table == ("0","0","1","1"):
                                if idx == 2:
                                    newids[tokens[0]] = inputs[1]
                                else:
                                    newids[tokens[0]] = inputs[2]
                                gates[str((old_truth_table,ins))] = newids[tokens[0]]
                            else:
                                gates[str((old_truth_table,ins))] = str(curline)
                                sys.stdout.write(str(curline) + " gate arity " + str(arity-1) + " table [ ")
                                for i in truth_table:
                                    sys.stdout.write(i + " ")
                                sys.stdout.write("] inputs [ ")
                                for q in inputs.keys():
                                    if q != idx:
                                        sys.stdout.write(str(inputs[q]) + " ")
                                sys.stdout.write("]")
                                if(n == 3):
                                    sys.stdout.write("\n")
                                else:
                                    sys.stdout.write(" "  + comment)
                                curline = curline + 1
                        else:
                            gates[str((truth_table,ins))] = str(curline)
                            sys.stdout.write(str(curline) + " ")
                            for i in range(1,s):
                                sys.stdout.write(tokens[i] + " ")
                            for q in inputs:
                                sys.stdout.write(str(inputs[q]) + " ")
                            sys.stdout.write("]")
                            if(n == 3):
                                sys.stdout.write("\n")
                            else:
                                sys.stdout.write(" "  + comment)
                            curline = curline + 1
                    elif truth_table == ("0", "1", "0", "1", "0", "0", "1", "1"):
                        sys.stdout.write(str(curline) + " gate arity 2 table [ 0 1 1 0 ] inputs [ "
                                         + str(inputs[0]) + " " + str(inputs[1]) + " ]\n")
                        curline = curline + 1
                        sys.stdout.write(str(curline) + " gate arity 2 table [ 0 0 0 1 ] inputs [ "
                                         + str(curline-1) + " " + str(inputs[2]) + " ]\n")
                        curline = curline + 1
                        sys.stdout.write(str(curline) + " gate arity 2 table [ 0 1 1 0 ] inputs [ "
                                         + str(curline-1) + " " + str(inputs[0]) + " ]\n")
                        newids[tokens[0]] = str(curline)
                        gates[str((truth_table,ins))] = str(curline)
                        curline = curline+1
                    elif truth_table == ("0", "0", "0", "1", "0", "1", "1", "1"):
                        sys.stdout.write(str(curline) + " gate arity 2 table [ 0 1 1 0 ] inputs [ "
                                         + str(inputs[0]) + " " + str(inputs[1]) + " ]\n")
                        curline = curline + 1
                        sys.stdout.write(str(curline) + " gate arity 2 table [ 0 1 1 0 ] inputs [ "
                                         + str(inputs[0]) + " " + str(inputs[2]) + " ]\n")
                        curline = curline + 1

                        sys.stdout.write(str(curline) + " gate arity 2 table [ 0 0 0 1 ] inputs [ "
                                         + str(curline-1) + " " + str(curline-2) + " ]\n")
                        curline = curline + 1

                        sys.stdout.write(str(curline) + " gate arity 2 table [ 0 1 1 0 ] inputs [ "
                                         + str(curline-1) + " " + str(inputs[0]) + " ]\n")
                        newids[tokens[0]] = str(curline)
                        gates[str((truth_table,ins))] = str(curline)
                        curline = curline+1
                    elif truth_table == ("0", "1", "1", "0", "1", "0", "0", "1"):
                        sys.stdout.write(str(curline) + " gate arity 2 table [ 0 1 1 0 ] inputs [ "
                                         + str(inputs[0]) + " " + str(inputs[1]) + " ]\n")
                        curline = curline + 1
                        sys.stdout.write(str(curline) + " gate arity 2 table [ 0 1 1 0 ] inputs [ "
                                         + str(curline-1) + " " + str(inputs[2]) + " ]\n")
                        newids[tokens[0]] = str(curline)
                        gates[str((truth_table,ins))] = str(curline)
                        curline = curline+1
                    else:
                        gates[str((truth_table,ins))] = str(curline)
                        sys.stdout.write(str(curline) + " ")
                        for i in range(1,s):
                            sys.stdout.write(tokens[i] + " ")
                        for q in inputs:
                            sys.stdout.write(str(inputs[q]) + " ")
                        sys.stdout.write("]")
                        if(n == 3):
                            sys.stdout.write("\n")
                        else:
                            sys.stdout.write(" "  + comment)
                        curline = curline + 1
                else:
                    sys.stdout.write(str(curline) + " ")
                    for i in range(1,s):
                        sys.stdout.write(tokens[i] + " ")
                    for q in inputs:
                        sys.stdout.write(str(inputs[q]) + " ")
                    sys.stdout.write("]")
                    if(n == 3):
                        sys.stdout.write("\n")
                    else:
                        sys.stdout.write(" "  + comment)
                    curline = curline + 1
