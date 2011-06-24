#
# Regular cron jobs for the libfn package
#
0 4	* * *	root	[ -x /usr/bin/libfn_maintenance ] && /usr/bin/libfn_maintenance
