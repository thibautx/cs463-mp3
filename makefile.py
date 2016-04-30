with open('largefile.txt', 'wb') as bigfile:
    bigfile.write('0'*100000000)