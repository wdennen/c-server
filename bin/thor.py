#!/usr/bin/env python3

import concurrent.futures
import os
import requests
import sys
import time

import argparse

# Functions

def usage(status=0):
    progname = os.path.basename(sys.argv[0])
    print(f'''Usage: {progname} [-h HAMMERS -t THROWS] URL
    -h  HAMMERS     Number of hammers to utilize (1)
    -t  THROWS      Number of throws per hammer  (1)
    -v              Display verbose output
    ''')
    sys.exit(status)

def hammer(url, throws, verbose, hid):
    ''' Hammer specified url by making multiple throws (ie. HTTP requests).

    - url:      URL to request
    - throws:   How many times to make the request
    - verbose:  Whether or not to display the text of the response
    - hid:      Unique hammer identifier

    Return the average elapsed time of all the throws.
    '''
    
    total = 0; 
    for throw in range(0, throws):
        
        t = time.time()
        r = requests.get(url)
        t = round(time.time() - t, 2)
        
        if verbose:
            print(r.text)
        
        print(f'Hammer: {hid}, Throw:   {throw}, Elapsed Time: {t}')
        total+=t
    
    avg = round(total / throws, 2)
    
    print(f'Hammer: {hid}, AVERAGE   , Elapsed Time: {avg}')
    
    return avg

def do_hammer(args):
    ''' Use args tuple to call `hammer` '''
    return hammer(*args)

def main():
    hammers     = 1
    throws      = 1
    verbose     = False
    total_time  = 0;
    
    # Parse command line arguments
    arguments   = sys.argv[1:]
    
    parser      = argparse.ArgumentParser(description='Thor', add_help=False)

    parser.add_argument('-h',
            type=int,
            dest='hammers',
            default=1,
            help='Number of hammers to utilize (1)')
    parser.add_argument('-t',
            type=int,
            dest='throws',
            default=1,
            help='Number of throws per hammer (1)')
    parser.add_argument('-v',
            action='store_true',
            dest='verbose',
            default=False,
            help='Display verbose output')
    parser.add_argument('url',
            type=str,
            help='Specified URL')
    
    try: 
        args = parser.parse_args()
    except:
        usage(1)

    # Create pool of workers and perform throws
    arguments = ((args.url, args.throws, args.verbose, hid) for hid in range(0, args.hammers));
    
    with concurrent.futures.ProcessPoolExecutor(args.hammers) as executor:
        time = executor.map(do_hammer, arguments)
    
    for t in time:
        total_time+=t
    
    total_avg = round(total_time/args.hammers, 2)
    
    print(f'TOTAL AVERAGE ELAPSED TIME: {total_avg}')
    
    
# Main execution

if __name__ == '__main__':
    main()

# vim: set sts=4 sw=4 ts=8 expandtab ft=python:
