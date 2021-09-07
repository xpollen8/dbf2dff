# dbf2dff
dBaseIII -> "Dfile" conversion utility, dating from 1992-12-15

# we'll allow the original code header to tell the story:
```
/*
	dbf2dff
		converts dBaseIII style .dbf/.dbt files into an ASCII
		file format used by the Dfile program and library of routines.
		usage: dbf2dff [-ghpPut -s # -o file -m name] file
		the dBase file is converted into Dfile files with suffix:
			.dff	-	equivalent to the .dbf+.dbt files.
			.dfa	-	contains logical to physical
					.dff record address mapping.
			.dfh	-	header descriptions for Dfile,
					if the -g flag is used.
			.dfw	-	window descriptions for Dfile,
					if the -g flag is used.
			.hlp	-	user-editable help template file,
					if the -h flag is used.
		flags:
		-g	will generate the Dfile-usable header
			description file (with extension .dfh),
			and the Dfile-usable window
			description file (with extension .dfw).
		-h	will generate user-editable Dfile
			help file template (with extension .hlp).
		-p	marks converted records as "protected" from change
			by the Dfile program.
		-P	marks converted files as "protected" from change
			by the Dfile program.
		-u	ignore deleted records in the dBase file.
			the default is to convert deleted records into
			undeleted Dfile records.
		-s	the dBase file is split into .dff databases whose
			names will be the 1st character in the `#'-th field.
			the dBase file will be converted into (up to) 28 Dfile
			databases, 26 for letters `a'-`z', 1 for numbers, and
			one for "other".
			if this flag is not used, the dBase file(s) will
			be converted into a single Dfile database with the
			same name as the dBase file.
		-m	if you convert several dBase files that have identical
			field descriptions, or use the -s flag,
			it is nice to assign a "model" name to those sets
			of files.  the Dfile program uses the model name
			to differentiate between a directory-full of related
			and unrelated .dff files.
			`file' is the default.
		-o	specify an output file.
			this is ignored if the -s and -m flags are used.
		-t	terse; do not show conversion progress.
	Dfile format explained
		.dff files:
			the Dfile file format consists of 79-character blocks,
			with the last 8 characters of each block holding the
			address of the next block in the record.  records are
			built from as many blocks are needed to hold the record
			data.  the last block of each record end with a -1.
			dBase memo fields are stored in the same manner.
			when a memo field is encountered in a record field,
			that field will contain the starting block of the memo
			record within the .dff file.
			the 8 character block pointer will allow for up to
			99999999 blocks per .dff file.
			insertion:
				the very first block of each .dff file contains
				a pointer to the next available free block.
				if this value is 0, the file will be appended.
			deletion:
				the data in the blocks for a deleted record
				(including its memo blocks, if used) are
				cleared and the free block is set to point
				to the deleted record block, whose last
				block pointer is set to 0.
			insertion and deletion in the .ddf file can cause the
			.dfa file to be updated to reflect changes in record
			starting block addresses.
		.dfa files:
			the .dfa files contain the starting blocks of each
			data record found in the companion .dff file.
			Dfile reads the .dfa file first and uses these block
			pointers to access .dff data.
		both the .ddf and .dfa files are ASCII files, which *can* be
		hand edited, as long as the block integrity is upheld.
	DOS limitations:
		although there may be up to 99999999 blocks in the .dff file,
		since DOS cannot allocate more than 32K at a time, DOS versions
		of Dfile are limited to having about 2000 actual records per
		.dff file..  if you are having problems with a large
		(> 1Meg dBase) file, try using the -s option, which will split
		the dBase file into (up to) 28 separate .dff/.dfa files.
		UNIX?  no problem at all.
	Dfile-v-dBase
		dBase uses fixed-length fields, Dfile does not limit the length
		of a data field at all.  since the .dff records are built of
		a list of blocks, Dfile can have variable-length data fields.
		when Dfile saves records to the .dff file, they typically
		take up much less space than dBase fixed-length records.
		memo field are typically an even bigger win.
		since dBase allocates 512 character per memo block, much
		of that space is un-used.  Dfile memo text takes up only
		as much space as they need.
		tests have shown space savings of 55% on a 5Meg dBase file.
		your savings will depend on how sparse your dBase file is;
		the less, the more Dfile wins.
	david whittemore - del@ecn.purdue.edu
	Tue Dec 15 02:14:18 EST 1992
 */
```
