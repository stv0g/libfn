0 0 * * * echo 'fnctl stop && fnctl fade -c 000000' | at $(sun rise 55 -6 1)
0 0 * * * echo 'fnctl start' | at $(sun set 55 -6 1)
