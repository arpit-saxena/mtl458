from sys import stdin
import pprint

arr = []
for line in stdin:
    s = line.strip().split()
    s[0] = int(s[0], 16) >> 12
    arr.append(s)
arr.append([-1, 'R'])

curr_page = arr[0][0]
read = False
write = False

for access in arr:
    [page, op] = access
        
    if page != curr_page or page == -1:
        print_op = ''
        if read:
            print_op += 'R'
        if write:
            print_op += 'W'
        print("0x%5x %s" % (curr_page, print_op))
        curr_page = page
        read = False
        write = False

    if op == 'R':
        read = True
    else:
        write = True