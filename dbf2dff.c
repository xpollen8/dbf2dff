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

#include	<stdio.h>
//#include	<malloc.h>	/* for malloc(), free() */
#include	<ctype.h>	/* for isascii() */
#include	<string.h>	/* for strncpy(), etc */
#include	<math.h>	/* for strncpy(), etc */

/*
	fixed DBASE constants
 */
#define	DBASE_MEMO_BLOCK	512	/* MEMO fld block size */
#define	DBASE_MEMO_END		26	/* memo records end with two of these */
#define	DBASE_MAX_MEMO_BLOCKS	4	/* MEMOs can have this many blocks */
#define	DBASE_HEADER_SIZE	32
#define	DBASE_LINE_FEED		10
#define	DBASE_CARRIAGE		13
#define	DBASE_COOKIE		0x3
#define	DBASE_MEMO_COOKIE	0x83
#define	DBASE_DELETED		'*'
#define	DBASE_DBF_EXT		"dbf"
#define	DBASE_DBT_EXT		"dbt"
#define	DBASE_CHARACTER_FLD	'C'	/* maps to Dfile ALP type */
#define	DBASE_LOGICAL_FLD	'L'	/* maps to Dfile ALP type */
#define	DBASE_DATE_FLD		'D'	/* maps to Dfile ALP type */
#define	DBASE_NUMERIC_FLD	'N'	/* maps to Dfile INT and FLT types */
#define	DBASE_MEMO_FLD		'M'	/* maps to Dfile MEMO type */
#define	DBASE_FLD_NAME_LEN	11	/* chars in field name */

char	tmp_byte[4];
#define	GetByte(f)	getc(f)
#define	GetInt(f)	(int)(fread((char *)tmp_byte, 1, 2, f), \
			dff_BytesToLong((char *)tmp_byte, 2))
#define	GetLong(f)	(long)(fread((char *)tmp_byte, 1, 4, f), \
			dff_BytesToLong((char *)tmp_byte, 4))

/*
	fixed Dfile constants
 */
#define	DF_VERSION_STRING	"Dfile01"	/* version */
#define DF_FREELIST		0	/* marks end of freelist in .dff file */
#define DF_REC_END		-1	/* marks end of record in .dff file */
#define DF_ADDR_WIDTH		8	/* space for "next address" */
#define	DF_BLOCK_LEN		79	/* length of of each .dff block */
#define	DF_REC_WIDTH		(DF_BLOCK_LEN - DF_ADDR_WIDTH - 1)
#define	DF_MAX_SPLIT		28	/* split files a-z,"other","numbers" */
#define	DF_OTHER_FILE		26	/* "other" file constant */
#define	DF_NUMBER_FILE		27	/* "other" file constant */
#define	DF_OTHER_NAME		"other"
#define	DF_NUMBER_NAME		"numbers"
#define	DF_NOT_SPLIT		-1	/* dBase file not being split */
#define	DF_TMP_EXT		"dft"	/* the database file extension */
#define	DF_DF_EXT		"dff"	/* the database file extension */
#define	DF_ADR_EXT		"dfa"	/* the address file extension */
#define	DF_HDR_EXT		"dfh"	/* the -g header file extension */
#define	DF_WIN_EXT		"dfw"	/* the -g window file extension */
#define	DF_HLP_EXT		"hlp"	/* the -h help file extension */
#define	DF_MAX_MEMO_SIZE	((DBASE_MAX_MEMO_BLOCKS * DBASE_MEMO_BLOCK) + 1)
#define	DF_DELIM		'\\'
#define	DF_DELIMS		"\\"
#define	DF_REPORT_DEFAULT	100
#define	DF_WIN_GEOM_SY		4	/* Dfile file window geometry */
#define	DF_WIN_GEOM_SX		9
#define	DF_WIN_GEOM_EY		12
#define	DF_WIN_GEOM_EX		74
#define	DF_TEXT_GEOM_SY		4	/* Dfile memo window geometry */
#define	DF_TEXT_GEOM_SX		9
#define	DF_TEXT_GEOM_EY		10
#define	DF_TEXT_GEOM_EX		49
#define	DF_SEARCH_ALL		"all"
#define	DF_SEARCH_INCLUSIVE	"incl"
#define	DF_WRITING_RECORD	0	/* flags for dff_WriteBlocks() */
#define	DF_WRITING_MEMO		1

#define	THIS_DIR		"."
#define	DF_SUCCESS			0	/* good exit */
#define	DF_FAILURE			1	/* bad exit */
#define	PROGNAME		"dbf2dff"
#define	FLAG_SET(f)		((f) == (unsigned)1)
#define	FLAG_NOT_SET(f)		((f) == (unsigned)0)

/*
	Dfile info used in converstion.
 */
typedef struct	{
	char	*in_file,		/* basename of .dbf/.dbt file(s) */
		*out_file,		/* basename of .dff/.dfa/.dfh/.dfw */
		*out_dir,		/* named output directory */
		*model,			/* Dfile model .dff files used with */
		*fld_buffer,		/* for decoding flds */
		*rec_buffer,		/* for holding input dBase records */
		*out_buffer,		/* for holding output Dfile records */
		*memo_buffer;		/* for writing memos */
	int	split,			/* fld to split on (or DF_NOT_SPLIT) */
		report,			/* tell progress */
		indx,			/* current .dff/.dfa file in use */
		num_flds,		/* # of dBase fields */
		*fld_type,		/* dBase field types */
		*fld_len,		/* dBase field lengths */
		*fld_dec,		/* dBase field decimal lengths */
		bytes;			/* bytes in the dBase record */
	struct {
		unsigned	headers : 1,		/* create header file */
				protect_file : 1,	/* protect Dfile file */
				protect_recs : 1,	/* protect Dfile recs */
				help : 1,		/* create help file */
				undel : 1,		/* undelete records */
				terse : 1;		/* terse mode */
	}	flags;
	long	num_records,		/* # of dBase records */
		rec_num,		/* current dBase record */
		logical[DF_MAX_SPLIT],	/* the last .dff rec read */
		physical[DF_MAX_SPLIT];	/* the last .dfa rec read */
	FILE	*dff,			/* .dff file pointer */
		*dfa,			/* .dfa/.dft file pointer */
		*dfh,			/* .dfh file pointer */
		*dfw,			/* .dfw file pointer */
		*hlp,			/* .hlp file pointer */
		*dbf,			/* dBase .dbf file handle */
		*dbt;			/* dBase .dbt file handle (or -1) */
}	DF_INFO;

/*
	prototypes
 */
#if defined(__STDC__) || defined(__cplusplus)
#       define  P_(s) s
#else
#       define  P_(s) ()
#endif
/*
	Dfile-ish routines.
 */
extern void	Dfile_WriteComment P_((FILE *, char *));
extern void	Dfile_WriteHeaderTop P_((DF_INFO *));
extern void	Dfile_WriteHeaderField P_((DF_INFO *, char *, int));
extern void	Dfile_WriteHeaderBottom P_((DF_INFO *));
extern void	Dfile_WriteHelpText P_((DF_INFO *, char *));
/*
	dbf2dff-ish routines.
 */
extern char	*dff_FileAndExt P_((char *, char *));
extern char	*dff_GenDfilename P_((DF_INFO *, char *));
extern void	dff_CleanUp P_((DF_INFO *, int));
extern void	dff_OutOfSpace P_((DF_INFO *));
extern long	dff_BytesToLong P_((char *, int));
extern void	dff_Open P_((DF_INFO *));
extern void	dff_WriteBlocks P_((DF_INFO *, char *, int));
extern int	dff_StripString P_((char **, int));
extern void	dff_TrimText P_((char **));
extern long	dff_DFTtoDFA P_((DF_INFO *, int));
extern void	dff_Init P_((DF_INFO *));
extern void	dff_Usage P_((void));
extern void	dff_DecodeArgs P_((DF_INFO *, int, char *[]));
extern void	main P_((int, char *[]));
/*
	dBase-ish routines.
 */
extern void	dBase_ProcessMemo P_((DF_INFO *, long));
extern void	dBase_ProcessRecord P_((DF_INFO *));
extern void	dBase_Init P_((DF_INFO *));
#undef	P_

#define	CheckDiskSpace(d, f) \
	if (ferror(f) != 0) dff_OutOfSpace(d)

/*+
	dff_CleanUp()

	Parameters
		`d' is the info struct.
		`status' is DF_SUCCESS or DF_FAILURE.

	Description
		called when horrible things happen, such as out of disk
		space, or dBase file corruption is detected.
		clean up and files created during the conversion and exit.
		returns exit code `status' to the OS.

	Calls
		System
			fclose(), fprintf(), free(), exit().
		Local
			dff_DFTtoDFA().

	Alters
		Incoming
			`d'.

	History
		dw	15 dec 92
 +*/
void	dff_CleanUp(d, status)
DF_INFO	*d;
int	status;
{
	/*
		close all open files.
	 */
	if (d->dff != (FILE *)NULL) fclose(d->dff);
	if (d->dfa != (FILE *)NULL) fclose(d->dfa);
	if (d->dfh != (FILE *)NULL) fclose(d->dfh);
	if (d->dfw != (FILE *)NULL) fclose(d->dfw);
	if (d->hlp != (FILE *)NULL) fclose(d->hlp);
	if (d->dbf != (FILE *)NULL) fclose(d->dbf);
	if (d->dbt != (FILE *)NULL) fclose(d->dbt);

	{
		long	num_converted = 0;

		for (d->indx = 0 ; d->indx < (d->split == DF_NOT_SPLIT ?
			1 : DF_MAX_SPLIT) ; d->indx++)
			/*
				add the Dfile header to the .dfa file(s).
			 */
			num_converted += dff_DFTtoDFA(d, status);

		if (status == DF_FAILURE) {
			if (FLAG_SET(d->flags.help))
				/*
					remove the generated help file.
				 */
				unlink(dff_FileAndExt(d->model, DF_HLP_EXT));
			if (FLAG_SET(d->flags.headers))
				/*
					remove the generated header file.
				 */
				unlink(dff_FileAndExt(d->model, DF_HDR_EXT));
			fprintf(stderr, "%s: exiting after %ld/%ld records.\n",
				PROGNAME, d->rec_num, d->num_records);
		} else if (FLAG_NOT_SET(d->flags.terse))
			printf("%s:%ld dBase records -> Dfile format\n",
				PROGNAME, num_converted);
	}
	/*
		be extra-nice.
	 */
	if (d->fld_type != (int *)NULL) free(d->fld_type);
	if (d->fld_dec != (int *)NULL) free(d->fld_dec);
	if (d->fld_len != (int *)NULL) free(d->fld_len);
	if (d->fld_buffer != (char *)NULL) free(d->fld_buffer);
	if (d->rec_buffer != (char *)NULL) free(d->rec_buffer);
	if (d->out_buffer != (char *)NULL) free(d->out_buffer);
	if (d->memo_buffer != (char *)NULL) free(d->memo_buffer);

	exit(status);
}

/*+
	dff_FileAndExt()

	Parameters
		`file' is the basename of the file.
		`ext' is the file extension.

	Description
		append `ext' to `file' and return a ptr to static space.

	Calls
		System
			sprintf().

	Return Values
		Explicit
			returns a pointer to static space.

	History
		dw	15 dec 92
 +*/
char    *dff_FileAndExt(file, ext)
char    *file,
	*ext;
{
	static char     tmp[100];
	sprintf(tmp, "%s.%s", file, ext);
	return (char *)tmp;
}

/*+
	dff_GenDfilename()

	Parameters
		`d' is the info struct.
		`ext' is the file extension.

	Description
		return a file name for the correct file as determined
		by the value of `d->indx'.

	Calls
		System
			strcpy(), sprintf().
		Local
			dff_FileAndExt().

	Return Values
		Explicit
			returns ptr to static space.

	History
		dw	15 dec 92
 +*/
char	*dff_GenDfilename(d, ext)
DF_INFO	*d;
char	*ext;
{
	static char     tmp[100];
	if (d->split == DF_NOT_SPLIT)
		strcpy(tmp, dff_FileAndExt(d->out_file, ext));
	else if (d->indx == DF_NUMBER_FILE)
		strcpy(tmp, dff_FileAndExt(DF_NUMBER_NAME, ext));
	else if (d->indx == DF_OTHER_FILE)
		strcpy(tmp, dff_FileAndExt(DF_OTHER_NAME, ext));
	else
		sprintf(tmp, "%c.%s", d->indx + 'a', ext);
	return (char *)tmp;
}

/*+
	dff_OutOfSpace()

	Parameters
		`d' is the info struct.

	Description
		called when horrible things happen, such as out of disk
		space, or when a file cannot be opened.

	Calls
		System
			fprintf().
		Local
			dff_CleanUp().

	History
		dw	15 dec 92
 +*/
void	dff_OutOfSpace(d)
DF_INFO	*d;
{
	fprintf(stderr, "\n%s: out of disk space!\n", PROGNAME);
	dff_CleanUp(d, DF_FAILURE);
}

/*+
	dff_BytesToLong()

	Parameters
		`byte' is a string whose values will be converted to long.
		`bytes' is the number of bytes of `byte' to convert.

	Description
		change the value held in `byte' to a long value.
		used to convert binary values into byte and word values.

	Return Values
		Explicit
			returns long representation of `byte'.

	History
		dw	15 dec 92
 +*/
long	dff_BytesToLong(byte, bytes)
char	*byte;
int	bytes;
{
	long	num = 0, tbit = 1;
	int	i, bit;
	for (i = 0 ; i < bytes ; i++, byte++)
		for (bit = 0 ; bit < 8 ; bit++, tbit *= 2)
			num += (tbit * ((*byte >> bit) & 1));

	return num;
}

/*+
	dff_Open()

	Parameters
		`d' is the info struct.

	Description
		open the .dff and .dft file pair specified by `d->indx'.
		Dfile record starting block information is written to
		.dft temp files which are converted to .dfa files by
		dff_DFTtoDFA() upon successful conversion of the entire
		dBase file.

	Calls
		System
			fopen(), fprintf().
		Local
			CheckDiskSpace().

	Alters
		Incoming
			`d->dff', `d->dfa'

	History
		dw	15 dec 92
 +*/
void	dff_Open(d)
DF_INFO	*d;
{
	if ((d->dff = fopen(dff_GenDfilename(d, DF_DF_EXT),
		(d->logical[d->indx] > 0L ?
		"a+" : "w"))) == (FILE *)NULL) dff_OutOfSpace(d);
	if ((d->dfa = fopen(dff_GenDfilename(d, DF_TMP_EXT),
		(d->logical[d->indx] > 0L ?
		"a+" : "w"))) == (FILE *)NULL) dff_OutOfSpace(d);

	if (d->logical[d->indx] == 0L) {
		/*
			this is the first time this file has been opened;
			output the FreeList information.
			NOTE: the created .dff file must have the same
			Model name as the .dfh file.
		 */
		fprintf(d->dff, "Version={%s} Model={%s}%*d\n",
			DF_VERSION_STRING, d->model,
			(DF_REC_WIDTH - (26 + strlen(d->model))) +
			DF_ADDR_WIDTH, DF_FREELIST);
		CheckDiskSpace(d, d->dff);
	}
}

/*+
	dff_WriteBlocks()

	Parameters
		`d' is the info struct.
		`ptr' holds an already-Dfile-formatted string.
		`which' is either DF_WRITING_RECORD or DF_WRITING_MEMO.

	Description
		writes the field-delimited string in the Dfile format
		to the .dff file and bumps the block pointer by the
		number of blocks written.

	Calls
		System
			strlen(), strncpy(), fprintf().
		Local
			dff_TrimText(), dff_Open(), CheckDiskSpace().

	Alters
		Incoming
			`d->dff', `d->physical[d->indx]'.

	Return Values
		Explicit
			returns exit code to the OS.

	History
		dw	15 dec 92
 +*/
void	dff_WriteBlocks(d, ptr, which)
DF_INFO	*d;
char	*ptr;
{
	int	len;

	if (d->dff == (FILE *)NULL)
		/*
			open the output files if not already open.
		 */
		dff_Open(d);

	if (which == DF_WRITING_RECORD) {
		/*
			write the starting record block to the .dft file.
		 */
		fprintf(d->dfa, "%c%ld\t%ld\n",
			(FLAG_SET(d->flags.protect_recs) ? '-' : ' '),
			++(d->logical[d->indx]), d->physical[d->indx] + 1L);
		CheckDiskSpace(d, d->dfa);
	} else
		/*
			remove special dBase chars and get into smallest space.
		 */
		dff_TrimText(&ptr);

	len = strlen(ptr);

	while (len > DF_REC_WIDTH) {
		/*
			split the formatted string into Dfile blocks.
		 */
		char	buffer[DF_REC_WIDTH + 1];
		strncpy(buffer, ptr, DF_REC_WIDTH);
		buffer[DF_REC_WIDTH] = '\0';
		fprintf(d->dff, "%s%*ld\n", buffer, DF_ADDR_WIDTH,
			(++(d->physical[d->indx]) + 1L));
		ptr += DF_REC_WIDTH;
		len -= DF_REC_WIDTH;
	}
	fprintf(d->dff, "%s%*d\n", ptr,
		(DF_REC_WIDTH - len) + DF_ADDR_WIDTH, DF_REC_END);
	CheckDiskSpace(d, d->dff);
	++(d->physical[d->indx]);
}

/*+
	dff_StripString()

	Parameters
		`str' points to the string to strip.
		`len' is the length of the string.

	Description
		removes leading and trailing spaces from `ptr'.

	Alters
		Incoming
			`ptr'.

	Return Values
		Explicit
			length of stripped `ptr'.

	History
		dw	15 dec 92
 +*/
int	dff_StripString(str, len)
char	**str;
int	len;
{
	/*
		remove leading and trailing whitespace.
	 */
	while (len > 0 && ((*str)[len - 1] == ' ' ||
		(*str)[len - 1] == DF_DELIM)) (*str)[--len] = '\0';
	while ((*str)[0] == ' ') (*str)++;

	return len;
}

/*+
	dff_TrimText()

	Parameters
		`ptr' is the Dfile string to trim.

	Description
		removes non-ASCII characters from dBase strings,
		removes multiple spaces and line feeds from the string
		in preparation for use by dff_WriteBlocks().

	Calls
		System
			isascii(), isspace(), isprint().
		Local
			dff_StripString().

	Alters
		Incoming
			`ptr'.

	History
		dw	15 dec 92
 +*/
void	dff_TrimText(ptr)
char	**ptr;
{
	int	i, len;

	/*
		translate end-of field markers
		and remove non-ascii dBase characters.
	 */
	for (i = 0 ; (*ptr)[i] != '\0' && (*ptr)[i] != DBASE_MEMO_END &&
		(*ptr)[i + 1] != DBASE_MEMO_END ; i++)
		if ((*ptr)[i] == DBASE_LINE_FEED || (*ptr)[i] == DBASE_CARRIAGE)
			(*ptr)[i] = DF_DELIM;
		else if (!isascii((*ptr)[i]) ||
			isspace((*ptr)[i]) || !isprint((*ptr)[i]))
			(*ptr)[i] = ' ';
	(*ptr)[(len = i) + 1] = '\0';
	len = dff_StripString(ptr, len);

	/*
		remove multiple blank lines and end-of-fields.
	 */
	for (i = 0 ; i < len ; i++)
		while (i < len && ((*ptr)[i] == DF_DELIM || (*ptr)[i] == ' ') &&
			((*ptr)[i + 1] == DF_DELIM || (*ptr)[i + 1] == ' ')) {
			int	l;
			for (l = i ; l < len ; l++)
				(*ptr)[l] = (*ptr)[l + 1];
			(*ptr)[len--] = '\0';
		}
}

/*+
	dBase_ProcessMemo()

	Parameters
		`d' is the info struct.
		`addr' is the dBase memo address.

	Description
		reads the dBase memo into a buffer which is passed
		onto dff_TrimText() for processing.  then calls
		dff_WriteBlocks() to add the memo text to the .dff file.

	Calls
		System
			fseek(), fread().
		Local
			dff_WriteBlocks().

	Alters
		Incoming
			`d'.

	Return Values
		Explicit
			returns exit code to the OS.

	History
		dw	15 dec 92
 +*/
void	dBase_ProcessMemo(d, addr)
DF_INFO	*d;
long	addr;
{
	char	*ptr = d->memo_buffer;

	if (d->dbt == (FILE *)NULL ||
		fseek(d->dbt, addr * (long)DBASE_MEMO_BLOCK, 0) != 0)
			/*
				either there *was* a memo field when no
				memos were specified by the .dbf magic cookie,
				or the address of the memo field is invalid.
				ignore the memo field for both cases.
			 */
			return;

	{
		/*
			read several MEMO blocks at a time.
			NOTE: if the MEMO field is more than
			DF_MAX_MEMO_SIZE long, it is truncated.
		 */
		int	bytes_read = fread(ptr, 1, DF_MAX_MEMO_SIZE, d->dbt);
		if (bytes_read == -1) {
			fprintf(stderr,
				"%s: couldn't read memo for record %ld\n",
				PROGNAME, d->rec_num + 1);
			dff_CleanUp(d, DF_FAILURE);
		}
		ptr[bytes_read] = '\0';
	}
	dff_WriteBlocks(d, ptr, DF_WRITING_MEMO);
}

/*+
	dBase_ProcessRecord()

	Parameters
		`d' is the info struct.

	Description
		reads the next dBase record and processes all of its
		fields, then writes them to the .dff file and updates
		the .dfa file with the starting .dff block of the Dfile record.

	Calls
		System
			fseek(), fread(), fprintf(), printf(), strncpy(),
			atof(), atol(), sprintf(), fclose(), isdigit(),
			tolower(), strcat(), fflush().
		Local
			dff_CleanUp(), dBase_ProcessMemo(), dff_TrimText(),
			CheckDiskSpace(), dff_WriteBlocks().

	Alters
		Incoming
			`d'.

	History
		dw	15 dec 92
 +*/
void	dBase_ProcessRecord(d)
DF_INFO	*d;
{
	int	i;
	char	*ptr = d->rec_buffer;
	long	rec_start = ftell(d->dbf);

	{
		int	bytes_read = fread(ptr, 1, d->bytes, d->dbf);
		if (bytes_read == -1) {
			fprintf(stderr, "%s: couldn't record %ld\n",
				PROGNAME, d->rec_num + 1);
			dff_CleanUp(d, DF_FAILURE);
		} else if (bytes_read != d->bytes) {
			fprintf(stderr, "\n%s: record %ld not %d bytes (%d)!\n",
				PROGNAME, d->rec_num, d->bytes, bytes_read);
			dff_CleanUp(d, DF_FAILURE);
		}
	}

	d->out_buffer[0] = ptr[d->bytes] = '\0';

	if ((int)ptr++ == DBASE_DELETED && FLAG_NOT_SET(d->flags.undel)) {
		/*
			not restoring deleted records.
			return.
		 */
		if (FLAG_NOT_SET(d->flags.terse))
			printf("\n%s: skipping %ld - use -u flag to keep\n",
				PROGNAME, d->rec_num);
		return;
	}

	/*
		get fields into Dfile format
	 */
	for (i = 0 ; i < d->num_flds ; i++) {
		char	*fld = d->fld_buffer;

		strncpy(fld, ptr, d->fld_len[i]);
		ptr += d->fld_len[i];
		fld[d->fld_len[i]] = '\0';

		if (d->fld_type[i] == DBASE_NUMERIC_FLD) {
			float	val = (float)atof(fld);
			/*
				get numbers into smallest
				possible space.
			 */
			if (val == (float)0)
				/*
					leave the field blank
				 */
				fld[0] = '\0';
			else
				sprintf(fld, "%g", val);
		} else if (d->fld_type[i] == DBASE_MEMO_FLD) {
			long	old_start = d->physical[d->indx];
			/*
				add the memo text to the .dff file
			 */
			dBase_ProcessMemo(d, (long)atol(fld));
			/*
				add the physical memo address to the memo field.
			 */
			sprintf(fld, "%ld",
				(d->physical[d->indx] == old_start ?
				DF_FREELIST : old_start + 1L));
		} else {
			/*
				remove special dBase chars and get into
				smallest space.
			 */
			dff_TrimText(&fld);

			if (i == d->split) {
				int	last = d->indx;

				/*
					set the indx file as the 1st char
					of the split field.
				 */
				if (isdigit(fld[0]))
					/*
						digit fields go into
						DF_NUMBER_FILE.
					 */
					d->indx = DF_NUMBER_FILE;
				else if ((d->indx =
					tolower(fld[0]) - 'a') < 0 ||
					d->indx >= DF_MAX_SPLIT)
					/*
						non-alpha go into the
						DF_OTHER_NAME file.
					 */
					d->indx = DF_OTHER_FILE;

				if (last != d->indx && d->dff != (FILE *)NULL) {
					/*
						if the field value has changed,
						re-set the dBase file to the
						start of this record.  (only if
						a .dff file is already in use).
					 */
					d->rec_num--;
					fseek(d->dbf, rec_start, 0);
					fclose(d->dff); fclose(d->dfa);
					d->dff = d->dfa = (FILE *)NULL;
					return;
				}
			}
		}

		strcat(d->out_buffer, fld);
		if (i < d->num_flds - 1)
			/*
				the last field is not delimited
			 */
			strcat(d->out_buffer, DF_DELIMS);
	}

	dff_WriteBlocks(d, d->out_buffer, DF_WRITING_RECORD);
	if (FLAG_NOT_SET(d->flags.terse) && (d->rec_num % d->report) == 0) {
		/*
			show percent done.
		 */
		static	int	percent_done = 0;
		printf("%d%% converted\n", percent_done++);
		fflush(stdout);
	}
}

/*+
	dff_DFTtoDFA()

	Parameters
		`d' is the info struct.
		`status' is DF_SUCCESS or DF_FAILURE.

	Description
		creates the .dfa file from the .dft temp file.

	Calls
		System
			strcpy(), printf(), fprintf(), fopen(), fclose(),
			unlink().
		Local
			dff_GenDfilename(), dff_OutOfSpace(), CheckDiskSpace().

	Return Values
		Explicit
			returns the number of records in the .dfa file.

	History
		dw	15 dec 92
 +*/
long	dff_DFTtoDFA(d, status)
DF_INFO	*d;
int	status;
{
	char	adr_file[100],
		tmp_file[100];

	strcpy(adr_file, dff_GenDfilename(d, DF_ADR_EXT));
	strcpy(tmp_file, dff_GenDfilename(d, DF_TMP_EXT));
	
	if (status == DF_SUCCESS && FLAG_NOT_SET(d->flags.terse))
		printf("%s has %ld records\n",
			dff_GenDfilename(d, DF_DF_EXT), d->logical[d->indx]);

	if (status == DF_SUCCESS && d->logical[d->indx] > 0L) {
		/*
			add the number of records to the top of the .dfa file.
		 */
		int	c;
		FILE	*tmp = fopen(adr_file, "w");
		if (tmp == (FILE *)NULL) dff_OutOfSpace(d);
		d->dfa = fopen(tmp_file, "r");
		Dfile_WriteComment(tmp, "Dfile Version");
		fprintf(tmp, "char\tVersion\t{%s}\n", DF_VERSION_STRING);
		Dfile_WriteComment(tmp, "Dfile Model name");
		fprintf(tmp, "char\tModel\t{%s}\n", d->model);
		fprintf(tmp, "char\tFileProtected\t{%s}\n",
			(FLAG_SET(d->flags.protect_file) ? "yes" : "no"));
		fprintf(tmp, "long\tNumRecords\t%ld\n", d->logical[d->indx]);
		Dfile_WriteComment(tmp, "a `-' marks a record as protected");
		fprintf(tmp, "long\tRecordAddresses[%ld]\n",
			d->logical[d->indx] * 2L);
		while ((c = fgetc(d->dfa)) != EOF) fputc(c, tmp);
		CheckDiskSpace(d, tmp);
		fclose(d->dfa);
		fclose(tmp);
		unlink(tmp_file);
	} else {
		/*
			exiting with DF_FAILURE status or
			no records for this letter;
			remove all associated Dfile files.
		 */
		unlink(tmp_file);
		unlink(dff_GenDfilename(d, DF_DF_EXT));
	}

	return d->logical[d->indx];
}

/*+
	dff_Init()

	Parameters
		`d' is the info struct.

	Description
		initialises the Dfile info struct.

	Alters
		Incoming
			`d'.

	History
		dw	15 dec 92
 +*/
void	dff_Init(d)
DF_INFO	*d;
{
	/*
		initialise stuff
	 */
	d->out_dir = d->model = d->in_file =
		d->out_file = d->fld_buffer = d->rec_buffer =
		d->out_buffer = d->memo_buffer = (char *)NULL;
	d->split = DF_NOT_SPLIT;
	d->indx = 0;
	d->flags.help = d->flags.headers =
		d->flags.protect_recs = d->flags.protect_file =
		d->flags.undel = d->flags.terse = (unsigned)0;
	d->hlp = d->dff = d->dfa = d->dfh = d->dfw =
		d->dbf = d->dbt = (FILE *)NULL;
	d->rec_num = d->num_records = 0L;
	{
		int	i;
		for (i = 0 ; i < DF_MAX_SPLIT ; i++)
			d->physical[i] = d->logical[i] = 0L;
	}
	d->fld_dec = d->fld_type = d->fld_len = (int *)NULL;
}

static char *use[] = {
	"usage: dbf2dff [-ghpPut -s # -o file -m name] file",
	"flags:",
	"g; generate Dfile header file during conversion",
	"h; generate Dfile help file template during conversion",
	"p; mark records as \"protected\" from editing via Dfile",
	"P; mark files as \"protected\" from editing via Dfile",
	"u; undelete dBase records during conversion",
	"s #; split into files based on field #",
	"o file; name an output file",
	"m model; give a name to a family of converted files",
	"t; terse/silent conversion",
	(char *)NULL
};

/*+
	dff_Usage()

	Description
		called when bad command-line args are used.
		show em what is valid. exits with code DF_FAILURE.

	Calls
		System
			fprintf(), fputc(), exit().

	History
		dw	15 dec 92
 +*/
void	dff_Usage()
{
	int	i = 0;
	while (use[i] != (char *)NULL) fprintf(stderr, "%s\n\t", use[i++]);
	fputc('\n', stderr);
	exit(DF_FAILURE);
}

/*+
	dff_DecodeArgs()

	Parameters
		`d' is the info struct.
		`argc' is the number of command-line arguments.
		`argv' are the command line args.

	Description
		set the appropriate flags and values in `d' as
		specified by the user.  call dff_Usage() if invalid
		command line is encountered.

	Calls
		System
			strlen(), fprintf(), atoi(), printf().
		Local
			dff_Usage().

	Alters
		Incoming
			`d'.

	History
		dw	15 dec 92
 +*/
void	dff_DecodeArgs(d, argc, argv)
DF_INFO	*d;
int	argc;
char	*argv[];
{
	int	i;

	if (argc < 2) dff_Usage();

	/*
		process the command line arguments.
		they can come in any order and can be concatenated.
	 */
	for (i = 1 ; i < argc ; i++)
		if (argv[i][0] == '-') {
			int	opt_len, opt_indx = 0;
			if (strlen(argv[i]) < 2) dff_Usage();
			opt_len = strlen(&argv[i][1]);
			while (opt_len-- > 0) {
				int	opt = argv[i][++opt_indx];
				switch (opt) {
					case 's':
					case 'o':
					case 'm':
					if (i == argc - 1) {
						fprintf(stderr,
					"%s: expected a value for flag `%c'\n",
						PROGNAME, opt);
						dff_Usage();
					}
					if (opt_len > 0) {
						fprintf(stderr,
					"%s: garbage after flag `%c'\n",
						PROGNAME, opt);
						dff_Usage();
					}
				}
				if (opt == 's')
					d->split = (int)atoi(argv[++i]);
				else if (opt == 'o')
					d->out_file = argv[++i];
				else if (opt == 'm')
					d->model = argv[++i];
				else if (opt == 'g')
					d->flags.headers = (unsigned)1;
				else if (opt == 'p')
					d->flags.protect_recs = (unsigned)1;
				else if (opt == 'P')
					d->flags.protect_file = (unsigned)1;
				else if (opt == 'h')
					d->flags.help = (unsigned)1;
				else if (opt == 'u')
					d->flags.undel = (unsigned)1;
				else if (opt == 't')
					d->flags.terse = (unsigned)1;
				else {
					fprintf(stderr, "%s: bad flag `%c'\n", 
						PROGNAME, opt);
					dff_Usage();
				}
			}
		} else {
			/*
				set the dBase filename.
				no wildcards, so the last free-standing
				argument found will be the input file used.
				since we allow the split option,
				this only makes sense.
			 */
			if (d->in_file != (char *)NULL)
				printf("%s: ignoring previous dBase file: %s\n",
					PROGNAME, d->in_file);
			d->in_file = argv[i];
		}

	if (d->in_file == (char *)NULL) {
		fprintf(stderr, "%s: no dBase file given\n", PROGNAME);
		dff_Usage();
	} else if (FLAG_NOT_SET(d->flags.terse))
		/*
			output file is specified
		 */
		printf("infile: %s.%s\n", d->in_file, DF_DF_EXT);

	if (d->out_file == (char *)NULL)
		/*
			converted database will have the same name as input.
		 */
		d->out_file = d->in_file;
	else if (FLAG_NOT_SET(d->flags.terse))
		/*
			output file is specified
		 */
		printf("outfile: %s.%s\n", d->out_file, DF_DF_EXT);

	if (d->model == (char *)NULL)
		/*
			no model name, out_file it will be.
		 */
		d->model = d->out_file;

	if (d->out_dir == (char *)NULL)
		/*
			the .dff/.dfa files will stay in current directory.
		 */
		d->out_dir = THIS_DIR;
	else
		/*
			feel free to add the process necessary to
			create an output directory.
			simply prepend the directory name to the
			.dff and .dfa files when opening/accessing them.
			code is already in place for placing the directory
			in to the .dfh header file.
		 */
		d->out_dir = THIS_DIR;
}

/*+
	Dfile_WriteComment()

	Parameters
		`fp' is the output file pointer.
		`text' is the comment string.

	Description
		write a DW format comment string to the output file.

	Calls
		System
			fprintf().

	History
		dw	15 dec 92
 +*/
void	Dfile_WriteComment(fp, text)
FILE	*fp;
char	*text;
{
	fprintf(fp, "#\n#\t%s\n#\n", text);
}

/*+
	Dfile_WriteHeaderTop()

	Parameters
		`d' is the info struct.

	Description
		the -g option was chose; a Dfile header file is being
		created.  open the .dfh file and write Dfile-specific
		things to it.

	Calls
		System
			fopen(), fprintf().
		Local
			dff_OutOfSpace(), CheckDiskSpace(), dff_FileAndExt().

	Alters
		Incoming
			`d->dfh'.

	History
		dw	15 dec 92
 +*/
void	Dfile_WriteHeaderTop(d)
DF_INFO	*d;
{
	if ((d->dfh = fopen(dff_FileAndExt(d->model,
		DF_HDR_EXT), "w")) == (FILE *)NULL)
		dff_OutOfSpace(d);
	Dfile_WriteComment(d->dfh, "Dfile Version");
	fprintf(d->dfh, "char\tVersion\t{%s}\n", DF_VERSION_STRING);
	Dfile_WriteComment(d->dfh, "Dfile Model name");
	fprintf(d->dfh, "char\tModel\t{%s}\n", d->model);
	Dfile_WriteComment(d->dfh, "Dfile introduction screens");
	fprintf(d->dfh, "int\tNumScreens\t2\n");
	fprintf(d->dfh, "char\tScreenNames[NumScreens]\n");
	fprintf(d->dfh, "{IntroScreen1}\t{IntroScreen2}\n");
	fprintf(d->dfh, "char\tIntroScreen1[2]\n");
	fprintf(d->dfh, "{dBase file `%s' converted by %s version %s}\n",
		d->in_file, PROGNAME, DF_VERSION_STRING);
	fprintf(d->dfh, "{for use with the %s model of Dfile}\n", d->model);
	fprintf(d->dfh, "char\tIntroScreen2[5]\n");
	fprintf(d->dfh, "{Dfile written 13 Dec 92 by:}\n");
	fprintf(d->dfh, "{David Whittemore - del@ecn.purdue.edu}\n");
	fprintf(d->dfh, "{National Soil Erosion Research Laboratory}\n");
	fprintf(d->dfh, "{West LaFayette, Indiana}\n");
	fprintf(d->dfh, "{ph: 317 494 8694}\n");
	fprintf(d->dfh, "char\tDataDirectory\t{%s}\n", d->out_dir);
	fprintf(d->dfh, "int\tNumFields\t%d\n", d->num_flds);
	fprintf(d->dfh,
		"#\n#\t{name}\t{%s.%s file look-up}\t{type}\t{len}\n#\n",
		d->model, DF_HLP_EXT);
	fprintf(d->dfh, "char\tModelFields[NumFields][4]\n");
	CheckDiskSpace(d, d->dfh);
}

/*+
	Dfile_WriteHelpText()

	Parameters
		`d' is the info struct.
		`ptr' is the field name.

	Description
		the -h option was chosen; a Dfile help file is being
		generated.  write Dfile-specific info to the help file.

	Calls
		System
			fprintf().
		Local
			CheckDiskSpace().

	History
		dw	15 dec 92
 +*/
void	Dfile_WriteHelpText(d, ptr)
DF_INFO	*d;
char	*ptr;
{
	fprintf(d->hlp, "disp %s\n", ptr);
	fprintf(d->hlp, "free-format help for field `%s'\n", ptr);
	fprintf(d->hlp, "can be edited in the ASCII file `%s.%s'\n",
		d->model, DF_HLP_EXT);
	fprintf(d->hlp, "$\n");
	CheckDiskSpace(d, d->hlp);
}

/*+
	Dfile_WriteHeaderField()

	Parameters
		`d' is the info struct.
		`fld_name' is the field name.
		`indx' is the field index.

	Description
		the -g option was chosen; a Dfile .dfh file is
		being generated.  add the Dfile-specific information
		for `fld_name' to the .dfh file.

	Calls
		System
			strcpy(), fprintf().
		Local
			CheckDiskSpace().

	History
		dw	15 dec 92
 +*/
void	Dfile_WriteHeaderField(d, fld_name, indx)
DF_INFO	*d;
char	*fld_name;
int	indx;
{
	/*
		write the header info.
	 */
	fprintf(d->dfh, "{%s}\t{%s}\t{%s}\t{%d}\n",
		fld_name,	/* the name of the field */
		fld_name,	/* the help file look-up value for the field */
		(d->fld_type[indx] == DBASE_NUMERIC_FLD ?	/* Dfile type */
			(d->fld_dec[indx] == 0 ? "INT" : "FLT") :
			(d->fld_type[indx] == DBASE_MEMO_FLD ? "MEMO" : "ALP")),
		/*
			the Dfile program needs the memo field to
			contain the ASCII representation of the field
			separator so that it can decode line breaks.
		 */
		(d->fld_type[indx] == DBASE_MEMO_FLD ?		/* field len */
			DF_DELIM :
		/*
			all other field contains their field width
		 */
		d->fld_len[indx]));

	CheckDiskSpace(d, d->dfh);
}

/*+
	Dfile_WriteHeaderBottom()

	Parameters
		`d' is the info struct.

	Description
		the -g option was chosen; a Dfile .dfh file is
		being generated.  finish-up Dfile-specific information
		for the .dfh file, and then close it.

	Calls
		System
			fclose(), fprintf().
		Local
			CheckDiskSpace().

	Alters
		Incoming
			`d->dfh'.

	History
		dw	15 dec 92
 +*/
void	Dfile_WriteHeaderBottom(d)
DF_INFO	*d;
{
	/*
		close the Dfile header file.
	 */
	fclose(d->dfh);

	if ((d->dfw = fopen(dff_FileAndExt(d->model,
		DF_WIN_EXT), "w")) == (FILE *)NULL)
		dff_OutOfSpace(d);
	/*
		write the Dfile window file.
	 */
	Dfile_WriteComment(d->dfh, "Dfile Version");
	fprintf(d->dfh, "char\tVersion\t{%s}\n", DF_VERSION_STRING);
	Dfile_WriteComment(d->dfh, "Dfile Model name");
	fprintf(d->dfh, "char\tModel\t{%s}\n", d->model);
	fprintf(d->dfw, "int\tUserListMax\t%d\n", d->num_flds);
	fprintf(d->dfw, "int\tNumWindows\t1\n");
	fprintf(d->dfw, "int\tTopWindow\t1\n");
	fprintf(d->dfw, "char\tWindowFile[NumWindows]\n{%s}\n",
		d->out_file);
	fprintf(d->dfw, "char\tWindowGeometry[NumWindows][4]\n");
	fprintf(d->dfw, "{%d}\t{%d}\t{%d}\t{%d}\n",
		DF_WIN_GEOM_SX, DF_WIN_GEOM_SY,
		DF_WIN_GEOM_EX, DF_WIN_GEOM_EY);
	fprintf(d->dfw, "char\tTextGeometry[NumWindows][4]\n");
	fprintf(d->dfw, "{%d}\t{%d}\t{%d}\t{%d}\n",
		DF_TEXT_GEOM_SX, DF_TEXT_GEOM_SY,
		DF_TEXT_GEOM_EX, DF_TEXT_GEOM_EY);
	fprintf(d->dfw, "char\tUserListSize[NumWindows]\n{%d}\n",
		d->num_flds);
	fprintf(d->dfw, "char\tUserList[NumWindows][UserListMax]\n");
	{
		/*
			write field ordering and search constraints.
		 */
		int	i;
		for (i = 0 ; i < d->num_flds ; i++) {
			fprintf(d->dfw,
				"{%d%c%d%c%s%c%c%c%s}\n",
				i + 1, DF_DELIM, i + 1, DF_DELIM,
				DF_SEARCH_ALL, DF_DELIM, DF_DELIM,
				DF_DELIM, DF_SEARCH_INCLUSIVE);
		}
	}
	CheckDiskSpace(d, d->dfw);
	fclose(d->dfw);
}

/*+
	dBase_Init()

	Parameters
		`d' is the info struct.

	Description
		attempt to open dBase files, check their integrity,
		and set-up Dfile field info.

	Calls
		System
			open(), fseek(), fprintf(), malloc(), printf(),
			fread(), fopen(), fclose().
		Local
			dff_FileAndExt(), dff_CleanUp() dff_BytesToLong(),
			Dfile_WriteHeaderTop(), Dfile_WriteHeaderField(),
			Dfile_WriteHeaderBottom(), Dfile_WriteHelpText(),
			dff_StripString(), dff_OutOfSpace().

	Alters
		Incoming
			`d'.

	History
		dw	15 dec 92
 +*/
void	dBase_Init(d)
DF_INFO	*d;
{
	unsigned char	cookie;

	if ((d->dbf = fopen(dff_FileAndExt(d->in_file, DBASE_DBF_EXT),
		"rb")) == (FILE *)NULL) {
		fprintf(stderr, "%s: cannot open dBase memo file `%s.dbt'\n",
			PROGNAME, d->in_file);
		dff_CleanUp(d, DF_FAILURE);
	}

	/*
		read the dBase header
	 */
	fseek(d->dbf, 0L, 0);
	
	if ((cookie = GetByte(d->dbf)) == DBASE_MEMO_COOKIE) {
		if (FLAG_NOT_SET(d->flags.terse)) printf("has MEMOs\n");
		if ((d->dbt = fopen(dff_FileAndExt(d->in_file,
			DBASE_DBT_EXT), "rb")) == (FILE *)NULL) {
			fprintf(stderr, "%s: cannot open memo file `%s.dbf'\n",
				PROGNAME, d->in_file);
			dff_CleanUp(d, DF_FAILURE);
		}
	} else if (cookie != DBASE_COOKIE) {
		fprintf(stderr, "%s: `%s.dbt' not dBase format.\n",
			PROGNAME, d->in_file);
		dff_CleanUp(d, DF_FAILURE);
	}

	/*
		skip past the date bytes
	 */
	GetByte(d->dbf); GetByte(d->dbf); GetByte(d->dbf);
	/*
		set up number of things and allocate buffers
	 */
	d->num_records = GetLong(d->dbf);
	d->fld_type = (int *)malloc(sizeof(int) * (d->num_flds =
		((GetInt(d->dbf) - DBASE_HEADER_SIZE) / DBASE_HEADER_SIZE)));
	d->fld_dec = (int *)malloc(sizeof(int) * d->num_flds);
	d->fld_len = (int *)malloc(sizeof(int) * d->num_flds);
	d->rec_buffer = (char *)malloc(sizeof(char) * ((d->bytes =
		GetInt(d->dbf)) + 1));
	d->out_buffer = (char *)
		malloc(sizeof(char) * (d->bytes + d->num_flds + 1));
	d->memo_buffer = (char *)malloc(sizeof(char) * (DF_MAX_MEMO_SIZE + 1));

	/*
		skip 20 reserved bytes
	 */
	GetLong(d->dbf); GetLong(d->dbf); GetLong(d->dbf);
	GetLong(d->dbf); GetLong(d->dbf);

	if (FLAG_NOT_SET(d->flags.terse)) {
		/*
			reporting..
		 */
		printf("%d fields per record\n", d->num_flds);
		printf("%ld records to process\n", d->num_records);
	}

	if (d->split != DF_NOT_SPLIT) {
		/*
			split fields are specified as 1..n, used as 0..n-1
		 */
		d->split--;
		if (d->split < 0 || d->split > d->num_flds) {
			fprintf(stderr, "%s: split field range: %d..%d\n",
				PROGNAME, 1, d->num_flds);
			dff_CleanUp(d, DF_FAILURE);
		}
	}

	if (FLAG_SET(d->flags.headers))
		/*
			if the header file is used, initialise it.
		 */
		Dfile_WriteHeaderTop(d);

	{
		/*
			get field info from dBase file.
		 */
		int	i, max_len = 0;

		if (FLAG_SET(d->flags.help))
			/*
				writing the help file template
			 */
			if ((d->hlp = fopen(dff_FileAndExt(d->model,
				DF_HLP_EXT), "w")) == (FILE *)NULL)
				dff_OutOfSpace(d);

		for (i = 0 ; i < d->num_flds ; i++) {
			char	name[DBASE_FLD_NAME_LEN + 1],
				*stripped_name = (char *)&name[0];

			if (fread((char *)name, 1, DBASE_FLD_NAME_LEN,
				d->dbf) != DBASE_FLD_NAME_LEN) {
				fprintf(stderr,
					"%s: problems reading field %d!\n",
					PROGNAME, i + 1);
				dff_CleanUp(d, DF_FAILURE);
			}
			dff_StripString(&stripped_name, DBASE_FLD_NAME_LEN);

			d->fld_type[i] = GetByte(d->dbf); GetLong(d->dbf);
			d->fld_len[i] = GetByte(d->dbf);
			d->fld_dec[i] = GetByte(d->dbf);
			/*
				skip 14 reserved bytes
			 */
			GetLong(d->dbf); GetLong(d->dbf);
			GetLong(d->dbf); GetInt(d->dbf);

			if (FLAG_SET(d->flags.help))
				/*
					write the help template for this field
				 */
				Dfile_WriteHelpText(d, stripped_name);

			if (d->fld_len[i] > max_len)
				max_len = d->fld_len[i];

			if (i == d->split && d->split != DF_NOT_SPLIT) {
				if (d->fld_type[i] != DBASE_CHARACTER_FLD) {
					fprintf(stderr,
					"%s: split field (%s) not CHAR type\n",
						PROGNAME, stripped_name);
					dff_CleanUp(d, DF_FAILURE);
				} else if (FLAG_NOT_SET(d->flags.terse))
					printf("splitting on (%s)\n",
						stripped_name);
			}
			if (FLAG_SET(d->flags.headers))
				/*
					write the header info for this field
				 */
				Dfile_WriteHeaderField(d, stripped_name, i);
		}
		d->fld_buffer = (char *)malloc(sizeof(char) * (max_len + 2));
		if (FLAG_SET(d->flags.help))
			fclose(d->hlp);
	}

	/*
		read the dBase end-of-header byte
	 */
	GetByte(d->dbf);

	if (FLAG_SET(d->flags.headers))
		/*
			finish up the .dfh and start the .dfw file.
		 */
		Dfile_WriteHeaderBottom(d);
}

/*+
	main()

	Parameters
		`argc' is the number of command-line arguments.
		`argv' are the command-line arguments.

	Description
		body of dbf2dff; set things up, process the command-line
		arguments, open the dBase files, process the dBase records,
		and clean-up.

	Calls
		System
			printf().
		Local
			dff_Init(), dff_DecodeArgs(), dBase_Init(),
			dBase_ProcessRecord(), Cleanup().

	History
		dw	15 dec 92
 +*/
void	main(argc, argv)
int	argc;
char	*argv[];
{
	DF_INFO	d;	/* handle to all information */

	/*
		set things up.
	 */
	dff_Init(&d);
	dff_DecodeArgs(&d, argc, argv);
	dBase_Init(&d);
	d.report = (int)(d.num_records / 100) + 1;
	/*
		process the records.
	 */
	for (d.rec_num = 0 ; d.rec_num < d.num_records ; d.rec_num++)
		dBase_ProcessRecord(&d);
	if (FLAG_NOT_SET(d.flags.terse))
		printf("100%% converted\n");
	/*
		and exit.
	 */
	dff_CleanUp(&d, DF_SUCCESS);
}
