# This AWK script is for use when the ERROR.TBL and ERROR.HLP files
# have entries rearranged and need to be renumbered. It sends a
# new version of the file to standard output, and also sends a
# new version of ERRORS.H to the file "errorsh"

BEGIN 	{
	printf"/* Errors.h */\n\nenum errornumber {" >"errorsh"
	count=0
	column=0
	reserved=24	# number of internal library errors allowed
	errors=149	# total number of errors; all others are warnings
	cols=5		# number of entries per line in ERRORS.H
	prefix="ERR"
	}

# A dummy entry with a long name field must be present to indicate the
# start of application specific errors. Count is switched to 24
# at this point (we presume that internal library errors are less
# than 24 in number).

/^@/{
	if ($2=="ENDLIBERRS") { count=reserved; column=0 }
	if ($2=="ENDERRS") { count=errors; column=0 }
	printf"\n@%d %s\n",count,$2
	if (column==0) printf"\n/* %-3d */ ",count>"errorsh"
	if (count==reserved || count==errors)
		printf"%s_%s=%d, ",prefix,$2,count >"errorsh"
	else	
		printf"%s_%s, ",prefix,$2 >"errorsh"
	if (++column==cols) column=0
	if (count==reserved) column=0
	if (count==errors) { column=0; prefix="WARN" }
	count++
	}

/^[^@]/{ print $0 }

END 	{
# The ERR_FINAL is just to go with the last ','; otherwise a syntax
# error occurs when trying to compile the header file.
	printf"\nERR_FINAL\n};\n\nvoid Error(enum errornumber errno,...);\n">"errorsh"
	}
