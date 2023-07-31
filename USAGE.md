***./qfsort*** 
has a number of arguments but the minimal is

./prcsort 

- **dataDir**  the location of input files to merge defaults to current dir

- **workingDir** the location where the output file is placed and it creates a subdir  
                 under this create intermediate files. Its called working because
                 it could be used to support recovery, i.e if the program encounters
                 any errors it can restart with last merged files.
                this defaults to tmpdir

-**outputFile** output file name if filename has a path component its used as is. If
                not the real path is outputDir/fileName. It defaults to MultiplexedFile.txt

-**outputDir** it defaults to workingDir

-**csvDelimiter** this maybe be needed if different form ','
The rest are for my own tuning you can also set it.

###
- ***./qfsort --datadir=/mnt/c/Users/prasa/Downloads/nyse***
- ***./qfsort --datadir=/mnt/c/Users/prasa/Downloads/nyse --outputFile=./test.txt***
- ***./qfsort --help***



