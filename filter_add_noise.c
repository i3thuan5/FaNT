/*
********************************************************************************
*
*      COPYRIGHT (C) 2004 Niederrhein University of Applied Sciences
*      47805 Krefeld, GERMANY, Tel Int + 49 172 8353212
*
********************************************************************************
*
*      File             : filter_add_noise.c
*      Author           : G. Hirsch
*      Tested Platforms : Linux-OS
*      Description      : The program can be used to
*                         - filter a set of speech signals  and/or
*                         - normalize the level of the speech signals and/or
*                         - add a noise signal to the set of speech signals.
*                         Filtering and level estimation are done with the
*                         ITU tools.
*                         Speech and noise data can be processed that have been
*                         sampled a 8 kHz or at 16 kHz.
*                         The levels of speech and noise (S and N) can be calculated
*                         after applying different filtering methods.
*                         For 16 kHz data:
*                         - S and N are calculated from the full frequency range from
*                           0 to 8 Khz (-> Option: -m=snr_8khz)
*                         For 8 and for 16 kHz data:
*                         - S and N are calculated from the full frequency range from
*                           0 to 4 Khz (-> Option: -m=snr_4khz)
*                         - S and N are calculated after applying an A-weighting filter
*                           (-> Option: -m=a_weight)
*                         - S and N are calculated after applying G.712 filtering to
*                           speech and noise (default). This mode has also been used
*                           for creating the noisy Aurora data.
*
*                         The program expects a list file as input containing
*                         all speech files to be processed. Furthermore a list file
*                         of same length is needed that defines the file names
*                         and pathes where to store the output data.
*                         Speech files have to be in raw SHORT format.
*
*      Note               An earlier version of this software has been used for
*                         generating the noisy speech data of the Aurora-2 and
*                         the Aurora-4 data base.
*
*      Revision history
*
*      Rev  Date       Name            Description
*      -------------------------------------------------------------------
*      p1a  30-11-99   G. Hirsch       basic version
*      p2a  11-05-01   G. Hirsch       extended for 16 kHz data by introducing
*                                      the filter mode p341_16k
*      p3a  19-11-04   G. Hirsch       different filtering methods applicable for
*                                      calculating the levels of speech and noise
*                                      optional compensation of DC in the signals for
*                                      calculating the levels of speech and noise
*      p4a  17-12-04   G. Hirsch       A-weighting filtering added
*      p5a  17-12-04   G. Hirsch       bug removed for the case of filtering 16 kHz
*                                      data with a P.341 characteristic
*      p6a  09-06-10   G. Hirsch       overflow check also in case of level normalization
*                                      only (suggested by D. Gelbart and J.I. Biel)
*
********************************************************************************
*/

/*
********************************************************************************
*                         MODULE INCLUDE FILE AND VERSION ID
********************************************************************************
*/
const char module_id[] = "@(#)$Id: $";

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "ugst-utl.h"
#include "iirflt.h"
#include "firflt.h"
#include "sv-p56.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#define NONE   9999
#define FILTER 0x1
#define NORM   0x2
#define ADD    0x4
#define SAMP16K    0x8
#define SNRANGE    0x10
#define SNR_4khz   0x20
#define SNR_8khz   0x40
#define DC_COMP    0x80
#define IND_LIST   0x100
#define A_WEIGHT   0x200

#define P341_FILTER_SHIFT  125
#define IRS_FILTER_SHIFT    75
#define MIRS_FILTER_SHIFT  182
#define P341_16K_FILTER_SHIFT  296

/*=====================================================================*/

enum { G712, P341, IRS, MIRS, G712_16K, P341_16K, DOWN };

typedef struct	{
		char  *input_list;
		char  *output_list;
		char  *noise_file;
		char  *index_list;
        	int    filter_type;
		float  norm_level;
		float  snr;
		float  snr_range;
		int    seed;
        	char  *log_file;
        	int    mode;
		} PARAMETER;



void anal_comline(PARAMETER*, int, char**);
void print_usage(char*);
void write_logfile(PARAMETER*, FILE*);
float* load_samples(FILE*, long *);
short* load_short_samples(FILE *, long *);
void filter_samples(float*, long, int);
void write_samples(float*, long, char*);
void DCOffsetFil(float*, long, int);
void AWeightFil(float*, long, int);
void process_one_file(PARAMETER,char *,char *,
     long,float *,float *,
	FILE *,FILE *);

/*=====================================================================*/

int  main(int argc, char *argv[])
{
	PARAMETER	pars;
	FILE       *fp_log, *fp_noise=NULL, *fp_list, *fp_outlist, *fp_index=NULL;
	long        no_noise_samples=0, i;
	float      *noise=NULL, *noise_g712=NULL;
	char        filename[300], out_filename[300];

	anal_comline(&pars, argc, argv);
	if ( (fp_log = fopen(pars.log_file, "a")) == NULL)
	{
		fprintf(stderr, "\ncannot open log file %s\n\n", pars.log_file);
		exit(-1);
	}
	write_logfile(&pars, fp_log);
	if (pars.mode & ADD)
	{
		if (pars.mode & IND_LIST)
		{
			if ( (fp_index = fopen(pars.index_list, "r")) == NULL)
			{
				fprintf(stderr, "\ncannot open index file %s\n\n", pars.index_list);
				exit(-1);
			}
		}
		if ( strstr(pars.noise_file, ".wav") )
		{
			unsigned int channels;
	    unsigned int sampleRate;
	    drwav_uint64 totalPCMFrameCount;
			noise = drwav_open_file_and_read_pcm_frames_f32(pars.noise_file, &channels, &sampleRate, &totalPCMFrameCount, NULL);
	    if (noise == NULL)
			{
				fprintf(stderr, "\ncannot open noise file %s\n\n", pars.noise_file);
				exit(-1);
	    }
			if (channels != 1)
			{
				fprintf(stderr, "\nnoise file must be mono %s\n\n", pars.noise_file);
				exit(-1);
			}
			if ((pars.mode & SAMP16K) && sampleRate != 16000)
			{
				fprintf(stderr, "\nsample rate must be 16000 %s\n\n", pars.noise_file);
				exit(-1);
			}
			else
			{
				fprintf(stderr, "\nsample rate must be 8000 %s\n\n", pars.noise_file);
				exit(-1);
			}
		}
		else {
			if ( (fp_noise = fopen(pars.noise_file, "r")) == NULL)
			{
				fprintf(stderr, "\ncannot open noise file %s\n\n", pars.noise_file);
				exit(-1);
			}
			/* load samples of noise signal twice
			   Buffer "noise_g712" only used for calculating noise level N  */
			noise = load_samples(fp_noise, &no_noise_samples);
			if ( ( noise_g712 = (float*)calloc((size_t)no_noise_samples, sizeof(float))) == NULL)
			{
				fprintf(stderr, "cannot allocate enough memory to buffer samples!\n");
				exit(-1);
			}
			fclose(fp_noise);
		}

		memcpy(noise_g712,noise,sizeof(float)*no_noise_samples);

		fprintf(fp_log, " %ld noise samples loaded from %s\n", no_noise_samples, pars.noise_file);

		/* filter noise signal in buffer "noise_g712" */
		if (pars.mode & SAMP16K)  /*  16 kHz data  */
		{
		    if (pars.mode & SNR_4khz)  /*  full 4 kHz bandwidth for calculating noise level N  */
		    {
			filter_samples(noise_g712, no_noise_samples, DOWN);  /*  downsampling  16 --> 8 kHz  */
			if (pars.mode & DC_COMP)
				DCOffsetFil(noise_g712, no_noise_samples/2, 8000);
		    }
		    else if (pars.mode & A_WEIGHT)
		    {
		        AWeightFil(noise_g712, no_noise_samples, 16000);
		    }
		    else if (pars.mode & SNR_8khz)
		    {
			if (pars.mode & DC_COMP)
				DCOffsetFil(noise_g712, no_noise_samples, 16000);
		    }
		    else
		    {
			filter_samples(noise_g712, no_noise_samples, G712_16K);  /*  G.712 filtering of 16 K data  */
			if (pars.mode & DC_COMP)
				DCOffsetFil(noise_g712, no_noise_samples/2, 8000);
		    }
		}
		else  /*  8 Khz data  */
		{
		    if (pars.mode & A_WEIGHT)  /* filtering with A-weighting curve */
			AWeightFil(noise_g712, no_noise_samples, 8000);
		    else if (!(pars.mode & SNR_4khz))  /* If NOT full 4 kHz bandwidth --> G.712 filtering  */
			filter_samples(noise_g712, no_noise_samples, G712);
		    if ( (pars.mode & DC_COMP) && (!(pars.mode & A_WEIGHT)) )
		    	DCOffsetFil(noise_g712, no_noise_samples, 8000);
		 }

		/* filter noise signal in buffer "noise" */
		if (pars.mode & FILTER)
		{
			filter_samples(noise, no_noise_samples, pars.filter_type);
			fprintf(fp_log, " Noise signal filtered\n");
		}
		if (pars.seed == -1)
		{
			srand((unsigned int) time(NULL));
			fprintf(fp_log, " random seed (actual time) for the extraction of the noise segment\n");
		}
		else
		{
			srand(pars.seed);
			fprintf(fp_log, " seed for the extraction of the noise segment: %d\n", pars.seed);
		}
	}
	fprintf(fp_log," ---------------------------------------------------------------------------\n");
	fprintf(fp_log, "Processing started ...\n");

	if ( pars.input_list == NULL)
	{
		if ( pars.output_list == NULL)
		{
			process_one_file(pars,NULL,NULL,
	              no_noise_samples,noise,noise_g712,
				fp_index,fp_log);
		}
		else if ( (fp_outlist = fopen(pars.output_list, "r")) == NULL)
		{
			fprintf(stderr, "\ncannot open list file %s\n", pars.output_list);
			exit(-1);
		}
		else
		{
			if ( fscanf(fp_outlist, "%s", out_filename) == EOF)
			{
				fprintf(stderr, "\nInsufficient number of files defined in output list!\n");
				exit(-1);
			}
			process_one_file(pars,NULL,out_filename,
	              no_noise_samples,noise,noise_g712,
				fp_index,fp_log);
			fclose(fp_outlist);
		}
	}
	else if ( (fp_list = fopen(pars.input_list, "r")) == NULL)
	{
		fprintf(stderr, "\ncannot open list file %s\n", pars.input_list);
		exit(-1);
	}
	else
	{
		if ( pars.output_list == NULL)
		{
			for (i=0 ; fscanf(fp_list, "%s", filename) != EOF ; i++)
			{
				if ( i>0 )
				{
					fprintf(stderr, "\nThere are more than one file defined in output list!\n");
					exit(-1);
				}
				process_one_file(pars,filename,NULL,
		              no_noise_samples,noise,noise_g712,
					fp_index,fp_log);
			}
		}
		else if ( (fp_outlist = fopen(pars.output_list, "r")) == NULL)
		{
			fprintf(stderr, "\ncannot open list file %s\n", pars.output_list);
			exit(-1);
		}
		else
		{
			while ( fscanf(fp_list, "%s", filename) != EOF)
			{
				if ( fscanf(fp_outlist, "%s", out_filename) == EOF)
				{
					fprintf(stderr, "\nInsufficient number of files defined in output list!\n");
					exit(-1);
				}
				process_one_file(pars,filename,out_filename,
		              no_noise_samples,noise,noise_g712,
					fp_index,fp_log);
			}
			fclose(fp_outlist);
		}
		fclose(fp_list);
	}

	fprintf(fp_log," --------------------------------------------------------------------------\n\n");
	fclose(fp_log);
	if (pars.mode & ADD)
	{
		free(noise_g712);
		free(noise);
		if (pars.mode & IND_LIST)
			fclose(fp_index);
	}
	return 0;
}


/*=====================================================================*/

void	anal_comline(PARAMETER *pars, int argc, char** argv)
{
	int c;
	extern	int optind;
	extern	char *optarg;

	pars->mode = 0;
	pars->input_list = NULL;
	pars->noise_file = NULL;
	pars->output_list = NULL;
	pars->index_list = NULL;
	pars->norm_level = NONE;
	pars->snr = NONE;
	pars->filter_type = NONE;
	pars->log_file = NULL;
	pars->seed = -1;

	if (argc == 1) /* no arguments */
	{
		print_usage(argv[0]);
	}

	while( (c = getopt(argc, argv, "udhi:o:n:f:m:l:s:r:w:e:a:")) != -1)
	{
	  /*  printf("Optind: %d Optarg: %s   c: %c\n", optind, optarg, c);  */
	  switch(c)
	  {
		case 'i':
			pars->input_list = optarg;
			if (access(pars->input_list, F_OK) == -1)
			{
				fprintf(stderr, "\nunable to access list file %s\n", pars->input_list);
				print_usage(argv[0]);
			}
			break;
		case 'n':
			pars->noise_file = optarg;
			if (access(pars->noise_file, F_OK) == -1)
			{
				fprintf(stderr, "\nunable to access noise file %s\n", pars->noise_file);
				print_usage(argv[0]);
			}
			pars->mode = pars->mode | ADD;
			break;
		case 'o':
			pars->output_list = optarg;
			if (access(pars->output_list, F_OK) == -1)
			{
				fprintf(stderr, "\nunable to access list file %s\n", pars->output_list);
				print_usage(argv[0]);
			}
			break;
		case 'a':
			pars->index_list = optarg;
			if (access(pars->index_list, F_OK) == -1)
			{
				fprintf(stderr, "\nunable to access index list file %s\n", pars->index_list);
				print_usage(argv[0]);
			}
			pars->mode = pars->mode | IND_LIST;
			break;
		case 'l':
			pars->norm_level = (float)atof(optarg);
			pars->mode = pars->mode | NORM;
			break;
		case 's':
			pars->snr = (float)atof(optarg);
			break;
		case 'w':
			pars->snr_range = (float)atof(optarg);
			pars->mode = pars->mode | SNRANGE;
			break;
		case 'f':
		    if ( strcmp(optarg, "g712") == 0 )       pars->filter_type = G712;
			else if (strcmp(optarg, "p341") == 0)  pars->filter_type = P341;
			else if (strcmp(optarg, "irs") == 0)  pars->filter_type = IRS;
			else if (strcmp(optarg, "mirs") == 0)  pars->filter_type = MIRS;
			else
			{
				fprintf(stderr,"\nunknown filter type ...\n");
				print_usage(argv[0]);
			}
			pars->mode = pars->mode | FILTER;
			break;
		case 'r':
			pars->seed = atoi(optarg);
			break;
		case 'u':
			pars->mode = pars->mode | SAMP16K;
			break;
		case 'd':
			pars->mode = pars->mode | DC_COMP;
			break;
		case 'm':
		        if ( strcmp(optarg, "snr_4khz") == 0 )
				pars->mode = pars->mode | SNR_4khz;
		        else if ( strcmp(optarg, "snr_8khz") == 0 )
				pars->mode = pars->mode | SNR_8khz;
		        else if ( strcmp(optarg, "a_weight") == 0 )
				pars->mode = pars->mode | A_WEIGHT;
			else
			{
				fprintf(stderr,"\nunknown mode for estimation of S and N ...\n");
				print_usage(argv[0]);
			}
			break;
		case 'e':
			pars->log_file = optarg;
			break;
		case 'h':
			print_usage(argv[0]);
		default:
			fprintf(stderr, "\n\nERROR: unknown option!\n");
			print_usage(argv[0]);
	   }
	}
	// if (pars->input_list == NULL)
	// {
	// 	fprintf(stderr, "\n\n Input list is not defined.");
	// 	print_usage(argv[0]);
	// }
	// if (pars->output_list == NULL)
	// {
	// 	fprintf(stderr, "\n\n Output list is not defined.");
	// 	print_usage(argv[0]);
	// }
	if ( (pars->mode & ADD) && (pars->snr == NONE) )
	{
		fprintf(stderr, "\n\n SNR not defined for noise adding.");
		print_usage(argv[0]);
	}
	if ((pars->mode == 0) || (pars->mode == SNR_4khz) || (pars->mode == SNR_8khz) || (pars->mode == A_WEIGHT))
	{
		fprintf(stderr, "\n\n Either noise adding nor filtering nor normalization defined!");
		print_usage(argv[0]);
	}
	if ((pars->mode & SAMP16K) && ((pars->filter_type == G712) || (pars->filter_type == IRS) || (pars->filter_type == MIRS)))
	{
		fprintf(stderr, "\n\n Processing of 16 kHz data can not be combined with G.712, IRS or MIRS filtering right now!");
		print_usage(argv[0]);
	}
	if (!(pars->mode & SAMP16K) && (pars->mode & SNR_8khz))
	{
		fprintf(stderr, "\n\n S and N can be estimated from the 8 kHz range only in case of processing 16 kHz data!");
		print_usage(argv[0]);
	}
	if (pars->log_file == NULL)
	{
		pars->log_file = "filter_add_noise.log";
	}
	if ((pars->mode & SAMP16K) && (pars->filter_type == P341))
	{
		pars->filter_type = P341_16K;
	}

}

/*=====================================================================*/

void print_usage(char *name)
{
	fprintf(stderr,"\nUsage:\t%s [Options]",name);
	fprintf(stderr,"\n\nOptions:");
	fprintf(stderr,"\n\t-i\t<filename> containing a list of speech files");
	fprintf(stderr,"\n\t-o\t<filename> containing a list of output speech files");
	fprintf(stderr,"\n\t-n\t<filename> referencing a noise file");
	fprintf(stderr,"\n\t\t(NOT giving a noise file means NO noise adding)");
	fprintf(stderr,"\n\t-u\tto indicate and enable processing of 16 kHz data");
	fprintf(stderr,"\n\t\t(Note: Only P.341 filtering can be applied in case of 16 kHz data!)");
	fprintf(stderr,"\n\t-m\t<mode> for estimating S and N");
	fprintf(stderr,"\n\t\t(possible modes are: snr_4khz or snr_8khz or a_weight)");
	fprintf(stderr,"\n\t\t(Note: S and N are estimated from the whole range up to 4 or up to 8 kHz");
	fprintf(stderr,"\n\t\t       or after applying an A-weighting filter)");
	fprintf(stderr,"\n\t\t(NOT defining the mode means S and N are estimated after G.712 filtering)");
	fprintf(stderr,"\n\t-d\tto enable DC offset compensation for calculating S and N");
	fprintf(stderr,"\n\t-f\t<type of filter>");
	fprintf(stderr,"\n\t\t(possible filters are: g712, p341, irs, mirs )");
	fprintf(stderr,"\n\t\t(NOT applying this option means NO filtering)");
	fprintf(stderr,"\n\t-l\t<value> of the desired normalization level");
	fprintf(stderr,"\n\t\t(NOT applying this option means NO normalization)");
	fprintf(stderr,"\n\t-s\t<value> of the desired SNR in dB");
	fprintf(stderr,"\n\t-w\t<value> of the desired SNR range");
	fprintf(stderr,"\n\t\t(defining this values -> SNR randomly chosen between)");
	fprintf(stderr,"\n\t\t(the value of the -s option and the sum of s+w)");
	fprintf(stderr,"\n\t-r\t<value> of the random seed");
	fprintf(stderr,"\n\t\t(NOT applying this option the seed is calculated from the actual time)");
	fprintf(stderr,"\n\t-e\t<filename> of logfile");
	fprintf(stderr,"\n\t-a\t<filename> of index list file");
	fprintf(stderr,"\n");
	exit(-1);
}

/*=====================================================================*/

void	write_logfile(PARAMETER *pars, FILE *fp)
{
    char *dum;
	time_t tt;

	tt = time(NULL);
	fprintf(fp,"Program started on: %s", ctime(&tt));
	fprintf(fp,"------------------------------------------------------\n");
	fprintf(fp," Input list file: %s\n", pars->input_list);
	fprintf(fp," Output list file: %s\n", pars->output_list);
	fprintf(fp," Log file: %s\n", pars->log_file);
	// fprintf(stdout,"Program started on: %s", ctime(&tt));
	// fprintf(stdout,"------------------------------------------------------\n");
	// fprintf(stdout," Input list file: %s\n", pars->input_list);
	// fprintf(stdout," Output list file: %s\n", pars->output_list);
	// fprintf(stdout," Log file: %s\n", pars->log_file);
	if (pars->mode & SAMP16K)
	{
		fprintf(fp," Processing of 16 kHz data\n");
		// fprintf(stdout," Processing of 16 kHz data\n");
	}
	else
	{
		fprintf(fp," Processing of 8 kHz data\n");
		// fprintf(stdout," Processing of 8 kHz data\n");
	}
	if (pars->mode & FILTER)
	{
		dum = "G712";
		switch (pars->filter_type)
		{
		  case P341:
			dum = "P341";
			break;
		  case IRS:
			dum = "IRS";
			break;
		  case MIRS:
			dum = "MIRS";
			break;
		  case P341_16K:
			dum = "P341";
			break;
		}
		fprintf(fp," Filtering speech (& noise) with a %s characteristic\n", dum);
		// fprintf(stdout," Filtering speech (& noise) with a %s characteristic\n", dum);
	}
	if (pars->mode & NORM)
	{
		fprintf(fp," Trying to normalize speech level to %6.2f dB\n", pars->norm_level);
		// fprintf(stdout," Trying to normalize speech level to %6.2f dB\n", pars->norm_level);
	}
	if (pars->mode & ADD)
	{
		fprintf(fp," Adding noise file %s at a SNR of %6.2f dB\n", pars->noise_file, pars->snr);
		// fprintf(stdout," Adding noise file %s at a SNR of %6.2f dB\n", pars->noise_file, pars->snr);
		if (pars->mode & SNR_8khz)  /* FULL 8 kHz bandwidth  */
		{
		   fprintf(fp," Speech and noise level are calculated from the frequency range 0 to 8 kHz\n");
		   // fprintf(stdout," Speech and noise level are calculated from the frequency range 0 to 8 kHz\n");
		}
		else if (pars->mode & SNR_4khz)  /* FULL 8 kHz bandwidth  */
		{
		   fprintf(fp," Speech and noise level are calculated from the frequency range 0 to 4 kHz\n");
		   // fprintf(stdout," Speech and noise level are calculated from the frequency range 0 to 4 kHz\n");
		}
		else if (pars->mode & A_WEIGHT)  /* A weighting filter */
		{
		   fprintf(fp," Speech and noise level are calculated after A-weighting filtering\n");
		   // fprintf(stdout," Speech and noise level are calculated after A-weighting filtering\n");
		}
		else   /* G.712  */
		{
		   fprintf(fp," Speech and noise level are calculated after G.712 filtering\n");
		   // fprintf(stdout," Speech and noise level are calculated after G.712 filtering\n");
		}
		if (pars->mode & DC_COMP)  /* DC compensation  */
		{
		   fprintf(fp," Speech and noise level are calculated from signals after DC compensation filtering\n");
		   // fprintf(stdout," Speech and noise level are calculated from signals after DC compensation filtering\n");
		}
	}
}


float *load_samples(FILE *fp, long *no_samples)
{
     short *buf;
	float *sig;

	buf=load_short_samples(fp,no_samples);

	if ( ( sig = (float*)calloc((size_t)*no_samples, sizeof(float))) == NULL)
	{
		fprintf(stderr, "cannot allocate enough memory to buffer samples!\n");
		exit(-1);
	}
	sh2fl_16bit(*no_samples, buf, sig, 1);
	free(buf);
	return(sig);
}

short *load_short_samples(FILE *fp, long *no_samples)
{
    short *buf;
    size_t unit = 1048576, readed;
    void *newPtr = NULL;
    *no_samples=0;

	if ( ( buf = (short*)calloc((size_t)unit, sizeof(short))) == NULL)
	{
		fprintf(stderr, "cannot allocate enough memory to buffer samples!\n");
		exit(-1);
	}

	while ( (readed = fread(buf+*no_samples, sizeof(short), (size_t)unit, fp)) != 0 )
	{
		*no_samples += readed;
		unit += unit;

		if ((newPtr = realloc(buf, (*no_samples + unit) * sizeof (short))) != NULL)
		    buf = (short*) newPtr;
		else
		{
			free(buf);
			fprintf(stderr, "cannot reallocate enough memory to buffer samples!\n");
			exit(-1);
		}
	}
	return (short*)realloc(buf, (*no_samples + 1)*sizeof(short));
}

void  write_samples(float *sig, long no_samples, char *name)
{
    FILE *fp;
	short *buf;

	if (name == NULL)
	{
		fp = stdout;
	}
	else if ( (fp = fopen(name, "w")) == NULL)
	{
		fprintf(stderr, "\ncannot open output file %s\n\n", name);
		exit(-1);
	}

	if ( ( buf = (short*)calloc((size_t)no_samples, sizeof(short))) == NULL)
	{
		fprintf(stderr, "cannot allocate enough memory to buffer samples!\n");
		exit(-1);
	}
	fl2sh_16bit(no_samples, sig, buf, 1);
	if ( fwrite(buf, sizeof(short), (size_t)no_samples, fp) != no_samples )
	{
		fprintf(stderr, "could not write all samples to file %s!\n", name);
		exit(-1);
	}
	free(buf);
	fclose(fp);
}

void filter_samples(float *signal, long no_samples, int type)
{
    CASCADE_IIR *g712_state;
	SCD_FIR     *p341_state, *irs_state, *mirs_state, *up_ptr, *down_ptr;
	float  *buf, *buf1, *buf2, *signal_buf;
	long    no=0, filter_shift=0;

    switch(type)
	{
	  case G712:
	  case G712_16K:
		filter_shift = 0;
		break;
	  case P341:
		filter_shift = P341_FILTER_SHIFT;
		break;
	  case IRS:
		filter_shift = IRS_FILTER_SHIFT;
		break;
	  case MIRS:
		filter_shift = MIRS_FILTER_SHIFT;
		break;
	  case P341_16K:
		filter_shift = P341_16K_FILTER_SHIFT;
		break;
	}

	if ( ( buf = (float*)calloc((size_t)(no_samples+filter_shift), sizeof(float))) == NULL)
	{
		fprintf(stderr, "cannot allocate enough memory to filter samples!\n");
		exit(-1);
	}
	if ( ( signal_buf = (float*)calloc((size_t)(no_samples+filter_shift), sizeof(float))) == NULL)
	{
		fprintf(stderr, "cannot reallocate enough memory to filter samples!\n");
		exit(-1);
	}
	memcpy(signal_buf, signal, (size_t)(no_samples*sizeof(float)));
	switch(type)
	{
	  case G712:
		g712_state = iir_G712_8khz_init();
		no = cascade_iir_kernel((no_samples+filter_shift), signal_buf, g712_state, buf);
		cascade_iir_free(g712_state);
		/* next lines only for testing the A weighting filter
		AWeightFil(signal_buf, no_samples, 16000);
		no = no_samples;
		memcpy(buf, signal_buf, (size_t)(no_samples*sizeof(float))); */
		break;
	  case P341:
		p341_state = fir_hp_8khz_init();
		no = hq_kernel((no_samples+filter_shift), signal_buf, p341_state, buf);
		hq_free(p341_state);
		break;
	  case IRS:
		irs_state = irs_8khz_init();
		no = hq_kernel((no_samples+filter_shift), signal_buf, irs_state, buf);
		hq_free(irs_state);
		break;
	  case MIRS:
		mirs_state = mod_irs_16khz_init();
		up_ptr = hq_up_1_to_2_init();
		down_ptr = hq_down_2_to_1_init();
		if ( ( buf1 = (float*)calloc((size_t)2*(no_samples+filter_shift), sizeof(float))) == NULL)
		{
			fprintf(stderr, "cannot allocate enough memory to filter samples!\n");
			exit(-1);
		}
		if ( ( buf2 = (float*)calloc((size_t)2*(no_samples+filter_shift), sizeof(float))) == NULL)
		{
			fprintf(stderr, "cannot allocate enough memory to filter samples!\n");
			exit(-1);
		}
		no = hq_kernel((no_samples+filter_shift), signal_buf, up_ptr, buf1);
		no = hq_kernel(2 * (no_samples+filter_shift), buf1, mirs_state, buf2);
		no = hq_kernel(2 * (no_samples+filter_shift), buf2, down_ptr, buf);
		hq_free (up_ptr);
		hq_free (mirs_state);
		hq_free (down_ptr);
		free(buf1);
		free(buf2);
		break;
	  case G712_16K:
		g712_state = iir_G712_8khz_init();
		down_ptr = hq_down_2_to_1_init();
		if ( ( buf1 = (float*)calloc((size_t)((no_samples+1)/2+filter_shift), sizeof(float))) == NULL)
		{
			fprintf(stderr, "cannot allocate enough memory to filter samples!\n");
			exit(-1);
		}
		no = hq_kernel((no_samples+filter_shift), signal_buf, down_ptr, buf1);
		no_samples /= 2;
		no = cascade_iir_kernel((no_samples+filter_shift), buf1, g712_state, buf);
		cascade_iir_free(g712_state);
		hq_free (down_ptr);
		free(buf1);
		break;
	  case P341_16K:
		p341_state = p341_16khz_init();
		no = hq_kernel((no_samples+filter_shift), signal_buf, p341_state, buf);
		hq_free(p341_state);
		break;
	  case DOWN:
		down_ptr = hq_down_2_to_1_init();
		no = hq_kernel((no_samples+filter_shift), signal_buf, down_ptr, buf);
		no_samples /= 2;
		hq_free (down_ptr);
		break;
	}
	if (no != (no_samples+filter_shift))
		fprintf(stderr, "Number of samples at output of filtering NOT equal to number of input samples!\n");
	memcpy(signal, &buf[filter_shift], (size_t)(no_samples*sizeof(float)));
	free(buf);
	free(signal_buf);
}

/***  DC offset compensation filtering  ***/
void DCOffsetFil(float *signal, long no_samples, int samp_freq)
{
  long i;
  float aux, prev_x, prev_y, coeff;

  if (samp_freq == 8000)
  {
  	/* y[n] = x[n] - x[n-1] + 0.999 * y[n-1] */
	coeff = 0.999;
  }
  else
  {
  	/* y[n] = x[n] - x[n-1] + 0.9995 * y[n-1] */
	coeff = 0.9995;
  }

  prev_x = 0.;
  prev_y = 0.;
  for (i=0 ; i<no_samples ; i++)
        {
          aux = signal[i];
          signal[i] = signal[i] - prev_x + 0.999 * prev_y;
          prev_x = aux;
          prev_y = signal[i];
        }
}

/***  A weighting filter  ***/
/* The filter characteristic of the A-weighting is realized as
   a combination of a 2nd order IIR HP filter and a FIR filter.
   The filters have been designed with MATLAB to match
   Ra(f) = 12200^2*f^4 / ( (f^2+20.6^2)*(f^2+12200^2)*sqrt(f^2+107.7^2)*sqrt(f^2+737.9^2) )
*/
void AWeightFil(float *signal, long no_samples, int samp_freq)
{
  long i, j;
  int nrfircoef, nr2;
  double aux, prev_x1, prev_y1, prev_x2, prev_y2, sig;
  double *b, *b1, *a1, *buf;
/* coefficients of HP IIR filter for 8 kHz */
double b1_8[3] = { 0.97803047920655972192, -1.95606095841311944383,  0.97803047920655972192};
double a1_8[3] = { 1.00000000000000000000, -1.95557824031503546536,  0.95654367651120331129};
/* coefficients of HP IIR filter for 16 kHz */
double b1_16[3] = { 0.98211268665798745481, -1.96422537331597490962,  0.98211268665798745481};
double a1_16[3] = { 1.00000000000000000000, -1.96390539174032729974,  0.96454535489162229744};
/* coefficients of FIR filter for 8 kHz */
double b_8[401] = {  \
	-0.00000048447483696946, -0.00000022512318749614, -0.00000026294025838101, \
	 0.00000001064770950293,  0.00000003833470677151,  0.00000051126078952589, \
	 0.00000069953309206104,  0.00000098541838241537,  0.00000081475746826278, \
	 0.00000071268849316663,  0.00000047296424495409,  0.00000045167560549269, \
	 0.00000025625324551665,  0.00000029659517075128,  0.00000012973089907994, \
	 0.00000021259865101836,  0.00000006731555126460,  0.00000020685105369918, \
	 0.00000011425817192679,  0.00000041296073931891,  0.00000044791493633252, \
	 0.00000070403634084571,  0.00000055460203484093,  0.00000059180030971632, \
	 0.00000037291369217390,  0.00000043930757040800,  0.00000022350428357547, \
	 0.00000032417428619399,  0.00000010931231083113,  0.00000023960656238246, \
	 0.00000002518603320581,  0.00000020360644134332,  0.00000002294875525823, \
	 0.00000036684242335179,  0.00000032206388036039,  0.00000067239882378009, \
	 0.00000047076577332115,  0.00000062355204864449,  0.00000033329059818567, \
	 0.00000049980041816919,  0.00000019054258683724,  0.00000038619575844507, \
	 0.00000005987624410169,  0.00000028352923650070, -0.00000006065134972245, \
	 0.00000021066197086678, -0.00000011559068267007,  0.00000035187682882953, \
	 0.00000018911184032816,  0.00000071185755567156,  0.00000039628928325198, \
	 0.00000071727603322036,  0.00000028502865756631,  0.00000061047935453455, \
	 0.00000013533215782697,  0.00000048963932090098, -0.00000002412770048109, \
	 0.00000035905116933378, -0.00000019714163211650,  0.00000023343303301514, \
	-0.00000032883174661595,  0.00000034112098540144, -0.00000002013183789564, \
	 0.00000076876433367610,  0.00000025451747786098,  0.00000083392066985720, \
	 0.00000016702345929555,  0.00000074451146044984,  0.00000000368726942094, \
	 0.00000061262145802736, -0.00000019658915230727,  0.00000044360091722352, \
	-0.00000044388652434922,  0.00000024103770006125, -0.00000069300296642645, \
	 0.00000028299231855406, -0.00000040858734051716,  0.00000077228655642159, \
	-0.00000007206267809928,  0.00000089818974605587, -0.00000014399904288098, \
	 0.00000082277817797847, -0.00000033588376942978,  0.00000066948742873521, \
	-0.00000060057886334042,  0.00000044146432002551, -0.00000096039560383583, \
	 0.00000012053414322732, -0.00000139567180432868,  0.00000003499817058916, \
	-0.00000120127431401377,  0.00000054929811663290, -0.00000084136392860322, \
	 0.00000071187362632263, -0.00000093656445044485,  0.00000062086550172766, \
	-0.00000120620094216959,  0.00000040541476845153, -0.00000159893445870286, \
	 0.00000006116939341500, -0.00000215817883874979, -0.00000046805016014766, \
	-0.00000291567502959354, -0.00000081034476310266, -0.00000295972826281522, \
	-0.00000038208445373341, -0.00000270158074324256, -0.00000028610530331721, \
	-0.00000295437553011978, -0.00000051213557187288, -0.00000346084232168490, \
	-0.00000093544688046448, -0.00000417377051206647, -0.00000157785035711933, \
	-0.00000517283917494427, -0.00000256013415986944, -0.00000658452041834079, \
	-0.00000348617114338415, -0.00000725051504153352, -0.00000348145442182330, \
	-0.00000748452095899366, -0.00000381670662519333, -0.00000833912676894361, \
	-0.00000460581537897684, -0.00000960803218461066, -0.00000575052259069264, \
	-0.00001126971690354723, -0.00000731471724310772, -0.00001347382962085700, \
	-0.00000953631917028709, -0.00001652423778949483, -0.00001204562230261030, \
	-0.00001898108292544097, -0.00001359523368806706, -0.00002102023713909460, \
	-0.00001567362600146969, -0.00002402916745522739, -0.00001859827094821024, \
	-0.00002792481555180809, -0.00002236632163891675, -0.00003278682614223167, \
	-0.00002716785941170217, -0.00003894078939793811, -0.00003350000474485616, \
	-0.00004707261529101495, -0.00004117953534261129, -0.00005549229201502203, \
	-0.00004854831975713259, -0.00006428602304202158, -0.00005751749178242488, \
	-0.00007547082178730966, -0.00006888306835026006, -0.00008937608730225377, \
	-0.00008304065366192661, -0.00010653315756608934, -0.00010069556565277376, \
	-0.00012791706451173652, -0.00012317597326295306, -0.00015534957920904628, \
	-0.00015119559113842406, -0.00018743511243768355, -0.00018315735451190256, \
	-0.00022492450567979631, -0.00022252894483161461, -0.00027191753804259159, \
	-0.00027210151946550513, -0.00033091855632192026, -0.00033462449100590694, \
	-0.00040528057920629279, -0.00041401099044859757, -0.00049996344641314633, \
	-0.00051634251110413301, -0.00062296124512021081, -0.00064870975228481043, \
	-0.00077929439175705522, -0.00081642092721146428, -0.00097961081175696862, \
	-0.00103615592056275404, -0.00124532298732607061, -0.00133067510583451858, \
	-0.00160455574020486129, -0.00173377904846212577, -0.00210208956880716599, \
	-0.00230122036337402887, -0.00281419951501742224, -0.00313155247940679650, \
	-0.00388062211113633822, -0.00440501290142665362, -0.00555548659253778630, \
	-0.00647018812272768650, -0.00837591414127070166, -0.01010231987300322896, \
	-0.01356061827384147031, -0.01708437456947442534, -0.02402258013383605159, \
	-0.03176179634779018046, -0.04725099052436917274, -0.06500419192239298427, \
	-0.10495713406978124382, -0.13001585049351535583,  1.00202934757439754421, \
	-0.13001585049351260803, -0.10495713406978261772, -0.06500419192239276223, \
	-0.04725099052436897151, -0.03176179634779023597, -0.02402258013383685303, \
	-0.01708437456947343655, -0.01356061827384207226, -0.01010231987300251599, \
	-0.00837591414127101912, -0.00647018812272795105, -0.00555548659253785135, \
	-0.00440501290142657816, -0.00388062211113645011, -0.00313155247940646083, \
	-0.00281419951501782339, -0.00230122036337384803, -0.00210208956880718334, \
	-0.00173377904846190763, -0.00160455574020492830, -0.00133067510583440777, \
	-0.00124532298732645832, -0.00103615592056261765, -0.00097961081175704668, \
	-0.00081642092721146472, -0.00077929439175693650, -0.00064870975228487461, \
	-0.00062296124512023910, -0.00051634251110382412, -0.00049996344641339472, \
	-0.00041401099044828212, -0.00040528057920655793, -0.00033462449100599172, \
	-0.00033091855632189261, -0.00027210151946566391, -0.00027191753804250599, \
	-0.00022252894483150621, -0.00022492450567991013, -0.00018315735451169751, \
	-0.00018743511243781582, -0.00015119559113829513, -0.00015534957920906756, \
	-0.00012317597326292961, -0.00012791706451191086, -0.00010069556565275803, \
	-0.00010653315756613178, -0.00008304065366182717, -0.00008937608730207478, \
	-0.00006888306835033379, -0.00007547082178731399, -0.00005751749178236594, \
	-0.00006428602304224104, -0.00004854831975686155, -0.00005549229201534046, \
	-0.00004117953534267964, -0.00004707261529099294, -0.00003350000474499829, \
	-0.00003894078939759813, -0.00002716785941168689, -0.00003278682614236486, \
	-0.00002236632163880632, -0.00002792481555207722, -0.00001859827094805007, \
	-0.00002402916745519642, -0.00001567362600153142, -0.00002102023713907599, \
	-0.00001359523368790508, -0.00001898108292530993, -0.00001204562230221977, \
	-0.00001652423778936245, -0.00000953631917073583, -0.00001347382962091341, \
	-0.00000731471724375662, -0.00001126971690382800, -0.00000575052259002190, \
	-0.00000960803218588184, -0.00000460581537765319, -0.00000833912676879278, \
	-0.00000381670662386180, -0.00000748452096203882, -0.00000348145442048370, \
	-0.00000725051504105103, -0.00000348617114306444, -0.00000658452041804100, \
	-0.00000256013416030796, -0.00000517283917536835, -0.00000157785035723070, \
	-0.00000417377051229723, -0.00000093544688056833, -0.00000346084232157659, \
	-0.00000051213557197883, -0.00000295437552986686, -0.00000028610530303880, \
	-0.00000270158074328942, -0.00000038208445346697, -0.00000295972826305478, \
	-0.00000081034476315829, -0.00000291567502957262, -0.00000046805016033679, \
	-0.00000215817883858096,  0.00000006116939340522, -0.00000159893445868161, \
	 0.00000040541476858004, -0.00000120620094225548,  0.00000062086550181404, \
	-0.00000093656445052059,  0.00000071187362625699, -0.00000084136392873181, \
	 0.00000054929811657256, -0.00000120127431400949,  0.00000003499817062538, \
	-0.00000139567180421291,  0.00000012053414316903, -0.00000096039560378374, \
	 0.00000044146432006458, -0.00000060057886339091,  0.00000066948742887569, \
	-0.00000033588376961814,  0.00000082277817799631, -0.00000014399904286968, \
	 0.00000089818974600899, -0.00000007206267791168,  0.00000077228655631520, \
	-0.00000040858734051298,  0.00000028299231857979, -0.00000069300296651602, \
	 0.00000024103770018627, -0.00000044388652449250,  0.00000044360091722030, \
	-0.00000019658915231848,  0.00000061262145802356,  0.00000000368726950959, \
	 0.00000074451146042521,  0.00000016702345934702,  0.00000083392066980561, \
	 0.00000025451747789236,  0.00000076876433370927, -0.00000002013183795176, \
	 0.00000034112098553517, -0.00000032883174674481,  0.00000023343303310152, \
	-0.00000019714163212011,  0.00000035905116921998, -0.00000002412770030070, \
	 0.00000048963932070069,  0.00000013533215787171,  0.00000061047935452411, \
	 0.00000028502865736597,  0.00000071727603351564,  0.00000039628928307606, \
	 0.00000071185755575744,  0.00000018911184033621,  0.00000035187682854291, \
	-0.00000011559068225173,  0.00000021066197066498, -0.00000006065134949705, \
	 0.00000028352923609170,  0.00000005987624482286,  0.00000038619575789876, \
	 0.00000019054258687088,  0.00000049980041800381,  0.00000033329059839068, \
	 0.00000062355204865072,  0.00000047076577331007,  0.00000067239882386183, \
	 0.00000032206388025069,  0.00000036684242346853,  0.00000002294875522055, \
	 0.00000020360644131705,  0.00000002518603326988,  0.00000023960656229525, \
	 0.00000010931231087879,  0.00000032417428617176,  0.00000022350428353128, \
	 0.00000043930757046438,  0.00000037291369211717,  0.00000059180030972943, \
	 0.00000055460203483238,  0.00000070403634081861,  0.00000044791493638141, \
	 0.00000041296073931221,  0.00000011425817192819,  0.00000020685105369570, \
	 0.00000006731555124485,  0.00000021259865104789,  0.00000012973089908160, \
	 0.00000029659517075261,  0.00000025625324549990,  0.00000045167560549383, \
	 0.00000047296424497843,  0.00000071268849317641,  0.00000081475746827296, \
	 0.00000098541838238358,  0.00000069953309205412,  0.00000051126078953913, \
	 0.00000003833470675649,  0.00000001064770952157, -0.00000026294025842014, \
	-0.00000022512318749586, -0.00000048447483693658 };
/* coefficients of FIR filter for 16 kHz */
double b_16[301] = {  \
	-0.00000163823566567235, -0.00000129349101568055, -0.00000173855867999297, \
	-0.00000138886083315020, -0.00000186074944599914, -0.00000150193139198946, \
	-0.00000200604496185638, -0.00000163127987422071, -0.00000217132557278059, \
	-0.00000176741808887155, -0.00000234382073006229, -0.00000183094344655654, \
	-0.00000250084283910075, -0.00000199864660566820, -0.00000262789002458754, \
	-0.00000215386043860660, -0.00000289217185109260, -0.00000239378523373080, \
	-0.00000322662206448115, -0.00000269434283679307, -0.00000362531450279619, \
	-0.00000305075217416252, -0.00000408817430627477, -0.00000346416689290217, \
	-0.00000461920770121783, -0.00000393932206526000, -0.00000522530308400259, \
	-0.00000448351321445257, -0.00000591564184061565, -0.00000510612699627384, \
	-0.00000670136346012773, -0.00000581822564278744, -0.00000759496947841621, \
	-0.00000663144143348438, -0.00000860841360320021, -0.00000755451505352295, \
	-0.00000974743477173025, -0.00000858432874211604, -0.00001099850081772816, \
	-0.00000951231180346016, -0.00001227514374058944, -0.00001069770708620096, \
	-0.00001351961693721276, -0.00001190082625789161, -0.00001508067422270440, \
	-0.00001334654683810277, -0.00001689161756572877, -0.00001502283369917000, \
	-0.00001895702883557266, -0.00001693675311730079, -0.00002129105878855550, \
	-0.00001910487054790810, -0.00002391661786573805, -0.00002155212349973529, \
	-0.00002686481719856254, -0.00002431145023492758, -0.00003017501508719200, \
	-0.00002742414172429308, -0.00003389545048083550, -0.00003094066284628364, \
	-0.00003808392712044369, -0.00003492107295288400, -0.00004280677257843587, \
	-0.00003943266482462276, -0.00004813086069643492, -0.00004454795915910790, \
	-0.00005411064105123142, -0.00004978108057728533, -0.00006053146558008195, \
	-0.00005591629066482250, -0.00006726077312005088, -0.00006247107842621700, \
	-0.00007501845211855596, -0.00006996429387079926, -0.00008383355250755138, \
	-0.00007848989593105642, -0.00009381300436270244, -0.00008815897211855582, \
	-0.00010508611888928092, -0.00009910600683043637, -0.00011781065580099466, \
	-0.00011149480231451605, -0.00013217771097594411, -0.00012552361433682034, \
	-0.00014841748019445592, -0.00014143186724447642, -0.00016680726333668429, \
	-0.00015950964206924115, -0.00018768220029857365, -0.00018011048605059599, \
	-0.00021144733901080876, -0.00020366775396491089, -0.00023858297765662684, \
	-0.00023076411910554420, -0.00026970431005075987, -0.00026068439401730485, \
	-0.00030470598160643134, -0.00029539153470893316, -0.00034372546700033225, \
	-0.00033441055096840278, -0.00038864335568094714, -0.00037937084585191045, \
	-0.00044046553493560527, -0.00043137871260839591, -0.00050044145280684585, \
	-0.00049173817484151965, -0.00057009787755619838, -0.00056205601515835267, \
	-0.00065134093406154082, -0.00064435205571055265, -0.00074658366829230961, \
	-0.00074120024680015531, -0.00085891975554849679, -0.00085592507999440650, \
	-0.00099237016459549198, -0.00099288457416451940, -0.00115224073327841446, \
	-0.00115788645226965956, -0.00134564572909820685, -0.00135881539184466910, \
	-0.00158226727446100215, -0.00160686124447576299, -0.00187589512010747717, \
	-0.00191361708126458496, -0.00224313077365659395, -0.00230285969530687976, \
	-0.00270869448954760448, -0.00280161130222293274, -0.00331284571947834715, \
	-0.00345545877593055615, -0.00411288499378428731, -0.00433074047660635814, \
	-0.00519546795915075323, -0.00552823600867017109, -0.00669413267342846511, \
	-0.00720362577421732476, -0.00881837437615939912, -0.00960052818708471110, \
	-0.01190264418816686619, -0.01310369434291633675, -0.01649112537915961921, \
	-0.01832242291243315474, -0.02349169549765109388, -0.02620808279888530226, \
	-0.03448966393581323620, -0.03812774396888992529, -0.05263240033584821315, \
	-0.05486415897428192912, -0.08877452476680040838, -0.01261484753501346777, \
	 1.00356872825050946751, -0.01261484753503031193, -0.08877452476679281723, \
	-0.05486415897428669614, -0.05263240033584432043, -0.03812774396889240247, \
	-0.03448966393581100881, -0.02620808279888764414, -0.02349169549764856813, \
	-0.01832242291243483048, -0.01649112537915836327, -0.01310369434291740708, \
	-0.01190264418816549055, -0.00960052818708617174, -0.00881837437615880758, \
	-0.00720362577421778446, -0.00669413267342735836, -0.00552823600867079299, \
	-0.00519546795915030307, -0.00433074047660710320, -0.00411288499378345464, \
	-0.00345545877593135715, -0.00331284571947772829, -0.00280161130222301470, \
	-0.00270869448954737420, -0.00230285969530739628, -0.00224313077365622315, \
	-0.00191361708126515785, -0.00187589512010717446, -0.00160686124447636755, \
	-0.00158226727446038416, -0.00135881539184473936, -0.00134564572909785601, \
	-0.00115788645226987423, -0.00115224073327792136, -0.00099288457416505543, \
	-0.00099237016459540590, -0.00085592507999461337, -0.00085891975554801476, \
	-0.00074120024680079542, -0.00074658366829181760, -0.00064435205571094784, \
	-0.00065134093406116699, -0.00056205601515879231, -0.00057009787755602219, \
	-0.00049173817484126367, -0.00050044145280631915, -0.00043137871260933070, \
	-0.00044046553493469183, -0.00037937084585278541, -0.00038864335568060160, \
	-0.00033441055096828900, -0.00034372546700017510, -0.00029539153470938310, \
	-0.00030470598160636098, -0.00026068439401741235, -0.00026970431005102376, \
	-0.00023076411910555691, -0.00023858297765633738, -0.00020366775396491997, \
	-0.00021144733901044975, -0.00018011048605068918, -0.00018768220029861783, \
	-0.00015950964206965665, -0.00016680726333593294, -0.00014143186724488663, \
	-0.00014841748019449227, -0.00012552361433689789, -0.00013217771097567499, \
	-0.00011149480231447122, -0.00011781065580081225, -0.00009910600683070414, \
	-0.00010508611888905710, -0.00008815897211896660, -0.00009381300436261302, \
	-0.00007848989593102117, -0.00008383355250745122, -0.00006996429387099301, \
	-0.00007501845211851302, -0.00006247107842627319, -0.00006726077311992514, \
	-0.00005591629066486161, -0.00006053146558001904, -0.00004978108057739705, \
	-0.00005411064105111847, -0.00004454795915914179, -0.00004813086069638520, \
	-0.00003943266482468589, -0.00004280677257856477, -0.00003492107295277574, \
	-0.00003808392712035809, -0.00003094066284633193, -0.00003389545048065900, \
	-0.00002742414172435651, -0.00003017501508728730, -0.00002431145023493965, \
	-0.00002686481719855991, -0.00002155212349960679, -0.00002391661786585488, \
	-0.00001910487054809171, -0.00002129105878838215, -0.00001693675311722188, \
	-0.00001895702883560177, -0.00001502283369927165, -0.00001689161756547775, \
	-0.00001334654683827559, -0.00001508067422280056, -0.00001190082625786406, \
	-0.00001351961693711764, -0.00001069770708634584, -0.00001227514374056167, \
	-0.00000951231180320429, -0.00001099850081810239, -0.00000858432874199897, \
	-0.00000974743477169181, -0.00000755451505353840, -0.00000860841360316040, \
	-0.00000663144143354042, -0.00000759496947838709, -0.00000581822564273562, \
	-0.00000670136346019077, -0.00000510612699626390, -0.00000591564184057631, \
	-0.00000448351321447595, -0.00000522530308401066, -0.00000393932206526943, \
	-0.00000461920770117965, -0.00000346416689293377, -0.00000408817430624457, \
	-0.00000305075217416854, -0.00000362531450277131, -0.00000269434283683277, \
	-0.00000322662206448276, -0.00000239378523369839, -0.00000289217185106983, \
	-0.00000215386043867678, -0.00000262789002454381, -0.00000199864660567825, \
	-0.00000250084283909178, -0.00000183094344656972, -0.00000234382073002803, \
	-0.00000176741808886073, -0.00000217132557281426, -0.00000163127987426894, \
	-0.00000200604496179164, -0.00000150193139199939, -0.00000186074944604085, \
	-0.00000138886083315537, -0.00000173855867992306, -0.00000129349101569237, \
	-0.00000163823566572743 };

  if (samp_freq == 8000)
  {
	b = b_8;
	b1 = b1_8;
	a1 = a1_8;
	nrfircoef = 401;
  }
  else
  {
	b = b_16;
	b1 = b1_16;
	a1 = a1_16;
	nrfircoef = 301;
  }

  nr2 = (int)nrfircoef/2;
  if ( ( buf = (double*)calloc((size_t)(no_samples+nrfircoef-1), sizeof(double))) == NULL)
  {
	fprintf(stderr, "cannot allocate enough memory to filter samples!\n");
	exit(-1);
  }

  /* IIR filter 2nd order */
  prev_x1 = 0.;
  prev_y1 = 0.;
  prev_x2 = 0.;
  prev_y2 = 0.;
  for (i=0 ; i<no_samples ; i++)
  {
  	aux = (double)signal[i];
	buf[i+nr2] = (double)signal[i]*b1[0] + prev_x1*b1[1] + prev_x2*b1[2];
	buf[i+nr2] -= (a1[1] * prev_y1 + a1[2] * prev_y2);
        prev_x2 = prev_x1;
        prev_x1 = aux;
        prev_y2 = prev_y1;
        prev_y1 = buf[i+nr2];
  }

  /* FIR filter */
  for (i=0 ; i<no_samples ; i++)
  {
	sig = 0.;
  	for (j=0 ; j<nrfircoef ; j++)
  	{
	   sig += b[j]*buf[i+j];
	}
	signal[i] = (float) sig;
  }
  free(buf);
}

void process_one_file(PARAMETER	pars,char *filename,char *out_filename,
	long no_noise_samples,float *noise,float *noise_g712,
	FILE *fp_index,FILE *fp_log)
{
	FILE        *fp_speech;
	long       no_speech_samples, no, start, i;
	float      *speech, *speech_two_pass, *noise_buf;
	SVP56_state volt_state;
	double      speech_level, noise_level, factor, fmax, snr;
	int         count;
		if (filename == NULL)
		{
			fp_speech = stdin;
		}
		else if ( (fp_speech = fopen(filename, "r")) == NULL)
		{
			fprintf(stderr, "\ncannot open speech file %s\n", filename);
			exit(-1);
		}

		/* load samples of speech signal for calculating speech level S */
		speech = load_samples(fp_speech, &no_speech_samples);
		if ( ( speech_two_pass = (float*)calloc((size_t)no_speech_samples, sizeof(float))) == NULL)
		{
			fprintf(stderr, "cannot allocate enough memory to buffer samples!\n");
			exit(-1);
		}
		memcpy(speech_two_pass,speech,sizeof(float)*no_speech_samples);

		if (pars.mode & SAMP16K)  /*  16 kHz data  */
		{
		    if (pars.mode & SNR_4khz)  /*  full 4 kHz bandwidth for calculating speech level S  */
		    {
			filter_samples(speech, no_speech_samples, DOWN);  /*  downsampling  16 --> 8 kHz  */
		    }
		    else if (pars.mode & A_WEIGHT)
		    {
		        AWeightFil(speech, no_speech_samples, 16000);
		    }
		    else if (pars.mode & SNR_8khz)
		    {
		    }
		    else
		    {
		  	filter_samples(speech, no_speech_samples, G712_16K); /*  G.712 filtering of 16 K data  */
		    }
		    if (pars.mode & SNR_8khz)  /* FULL 8 kHz bandwidth */
		    {
			init_speech_voltmeter(&volt_state, 16000.);
			if (pars.mode & DC_COMP)
				DCOffsetFil(speech, no_speech_samples, 16000);
		  	speech_level = speech_voltmeter(speech, no_speech_samples, &volt_state);
		    }
		    else
		    {
			init_speech_voltmeter(&volt_state, 8000.);
			if ( (pars.mode & DC_COMP) && (!(pars.mode & A_WEIGHT)) )
				DCOffsetFil(speech, no_speech_samples/2, 8000);
		  	speech_level = speech_voltmeter(speech, no_speech_samples/2, &volt_state);
		    }
		}
		else  /*  8 kHz data  */
		{
		    if (pars.mode & A_WEIGHT)  /* filtering with A-weighting curve */
			AWeightFil(speech, no_speech_samples, 8000);
		    else if (!(pars.mode & SNR_4khz))  /* If NOT full 4 kHz bandwidth --> G.712 filtering  */
		  	filter_samples(speech, no_speech_samples, G712);
		    init_speech_voltmeter(&volt_state, 8000.);
		    if ( (pars.mode & DC_COMP) && (!(pars.mode & A_WEIGHT)) )
			DCOffsetFil(speech, no_speech_samples, 8000);
		    speech_level = speech_voltmeter(speech, no_speech_samples, &volt_state);
		}
		if (filename ==NULL)
		{
			fprintf(fp_log, " file:stdin  s-level:%6.2f  ", speech_level);
		}
		else
		{
			fprintf(fp_log, " file:%s  s-level:%6.2f  ", &filename[i+1], speech_level);
		}

		free(speech);

		/* load samples of speech signal again */
		speech = speech_two_pass;

		/* filter speech signal */
		if (pars.mode & FILTER)
		{
		    filter_samples(speech, no_speech_samples, pars.filter_type);
		}

		/* normalize level of speech signal to desired level  */
		if (pars.mode & NORM)
		{
			factor = pow(10., (pars.norm_level - speech_level)/20.);
			scale(speech, no_speech_samples, factor);
			speech_level = pars.norm_level;
		}

		if (pars.mode & ADD)  /*  Noise adding  */
		{
			if ( ( noise_buf = (float*)calloc((size_t)no_speech_samples, sizeof(float))) == NULL)
			{
				fprintf(stderr, "cannot allocate enough memory to buffer noise samples!\n");
				exit(-1);
			}
			if (no_noise_samples > no_speech_samples)  /* noise signal longer than speech signal */
			{
				/* select segment randomly out of noise signal */
				if (pars.mode & IND_LIST)
				{
				   if ( fscanf(fp_index, "%ld", &start) == EOF)
				   {
					fprintf(stderr, "\nInsufficient number of indices defined in index list file!\n");
					exit(-1);
				   }
				}
				else
				   start = (long) ( (double)(rand())/(RAND_MAX) * (double)(no_noise_samples - no_speech_samples));

				fprintf(fp_log, "1st noise sample:%ld  ", start);

				/* calculate noise level of selected segment  */
				if (pars.mode & SAMP16K)  /*  16 kHz data  */
				{
		    		    if (pars.mode & SNR_8khz)  /* calculate noise level from 16 kHz data  */
		    		    {
				  	memcpy(noise_buf, &noise_g712[start], (size_t)(no_speech_samples*sizeof(float)));
					init_speech_voltmeter(&volt_state, 16000.);
				  	speech_voltmeter(noise_buf, no_speech_samples, &volt_state);
		    		    }
		    		    else  /* calculate noise level from downsampled 8 kHz data  */
		    		    {
				  	memcpy(noise_buf, &noise_g712[start/2], (size_t)(no_speech_samples/2*sizeof(float)));
					init_speech_voltmeter(&volt_state, 8000.);
				  	speech_voltmeter(noise_buf, no_speech_samples/2, &volt_state);
				    }
				}
				else  /*  8 kHz data  */
				{
				   memcpy(noise_buf, &noise_g712[start], (size_t)(no_speech_samples*sizeof(float)));
				   init_speech_voltmeter(&volt_state, 8000.);
				   speech_voltmeter(noise_buf, no_speech_samples, &volt_state);
				}

				noise_level = SVP56_get_rms_dB(volt_state);
				fprintf(fp_log, "n-level:%6.2f", noise_level);
				memcpy(noise_buf, &noise[start], (size_t)(no_speech_samples*sizeof(float)));
			}
			else /* speech signal longer than noise signal */
			     /* use noise signal several times by starting with the 1st sample again at the end  */
			{
				no = 0;
				if (pars.mode & SAMP16K)  /*  16 kHz data  */
				{
				  if (pars.mode & SNR_8khz)  /* FULL 8 kHz bandwidth  */
				  {
				     while (no < no_speech_samples)
				     {
					if ((no_speech_samples-no) > no_noise_samples)
					{
						memcpy(&noise_buf[no], noise_g712,(size_t)(no_noise_samples*sizeof(float)));
						no += no_noise_samples;
					}
					else
					{
						memcpy(&noise_buf[no], noise_g712,(size_t)((no_speech_samples-no)*sizeof(float)));
						no = no_speech_samples;
					}
				     }
				     init_speech_voltmeter(&volt_state, 16000.);
				     speech_voltmeter(noise_buf, no_speech_samples, &volt_state);
				  }
				  else  /* in case of 4 kHz bandwidth with or without G.712 filtering  */
				        /* process downsampled version of noise signal */
				  {
				    while (no < no_speech_samples/2)
				    {
					if ((no_speech_samples/2-no) > no_noise_samples/2)
					{
						memcpy(&noise_buf[no], noise_g712,(size_t)(no_noise_samples/2*sizeof(float)));
						no += no_noise_samples/2;
					}
					else
					{
						memcpy(&noise_buf[no], noise_g712,(size_t)((no_speech_samples/2-no)*sizeof(float)));
						no = no_speech_samples/2;
					}
				    }
				    init_speech_voltmeter(&volt_state, 8000.);
				    speech_voltmeter(noise_buf, no_speech_samples/2, &volt_state);
				  }
				}
				else  /*  8 kHz data  */
				{
				  while (no < no_speech_samples)
				  {
					if ((no_speech_samples-no) > no_noise_samples)
					{
						memcpy(&noise_buf[no], noise_g712,(size_t)(no_noise_samples*sizeof(float)));
						no += no_noise_samples;
					}
					else
					{
						memcpy(&noise_buf[no], noise_g712,(size_t)((no_speech_samples-no)*sizeof(float)));
						no = no_speech_samples;
					}
				  }
				  init_speech_voltmeter(&volt_state, 8000.);
				  speech_voltmeter(noise_buf, no_speech_samples, &volt_state);
				}

				noise_level = SVP56_get_rms_dB(volt_state);
				fprintf(fp_log, "noise too short! n-level:%6.2f", noise_level);
				no = 0;
				while (no < no_speech_samples)
				{
					if ((no_speech_samples-no) > no_noise_samples)
					{
						memcpy(&noise_buf[no], noise, (size_t)(no_noise_samples*sizeof(float)));
						no += no_noise_samples;
					}
					else
					{
						memcpy(&noise_buf[no], noise, (size_t)((no_speech_samples-no)*sizeof(float)));
						no = no_speech_samples;
					}
				}
			}
			if (pars.mode & SNRANGE)
			{
			  snr = (double)pars.snr + ( (double)(rand())/(double)(RAND_MAX) * (double)(pars.snr_range) );
			  fprintf(fp_log, "  SNR:%f", snr);
			}
			else
			  snr = pars.snr;
			factor = pow(10., ((speech_level - snr) - noise_level)/20.);
			scale(noise_buf, no_speech_samples, factor);
			/*fmax = 0.; */
			for (i=0; i<no_speech_samples; i++)
			{
				speech[i] += noise_buf[i];
				/*if (fabs((double)speech[i]) > fmax)
					fmax = fabs((double)speech[i]);*/
			}
			/*if (fmax > 1.)
			{
				fprintf(fp_log, "\n ATTENTION!!! overload: %6.2f", fmax);
				for (i=0; i<no_speech_samples; i++)
					speech[i] /= (float)fmax;
			} */

			free(noise_buf);
		}
		/* The overload check has been moved here!
		   Now the check is also done in case of a level normalization only! */
		fmax = 0.;
		for (i=0; i<no_speech_samples; i++)
		{
			if (fabs((double)speech[i]) > fmax)
				fmax = fabs((double)speech[i]);
		}
		if (fmax > 1.)
		{
			fprintf(fp_log, "\n ATTENTION!!! overload by factor %6.2f", fmax);
			for (i=0; i<no_speech_samples; i++)
				speech[i] /= (float)fmax;
			if (pars.mode & NORM)
			{
				// fprintf(stdout, "ATTENTION !!!\n" );
				// fprintf(stdout, " Due to overload the speech level could only be normalized to %6.2f\n", pars.norm_level - 20*log10(fmax));
				fprintf(fp_log, "\n Due to overload the speech level could only be normalized to %6.2f", pars.norm_level - 20*log10(fmax));
			}
		}

		if ( strstr(out_filename, ".wav") )
		{
			drwav dwav;
			drwav_data_format format;
			format.container = drwav_container_riff;     // <-- drwav_container_riff = normal WAV files, drwav_container_w64 = Sony Wave64.
			format.format = DR_WAVE_FORMAT_PCM;          // <-- Any of the DR_WAVE_FORMAT_* codes.
			format.channels = 1;
			format.sampleRate = (pars.mode & SAMP16K) ? 16000 : 8000;
			format.bitsPerSample = 16;
			drwav_init_file_write(&dwav, out_filename, &format, NULL);
			short *buf;
			if ( ( buf = (short*)calloc((size_t)no_speech_samples, sizeof(short))) == NULL)
			{
				fprintf(stderr, "cannot allocate enough memory to buffer samples!\n");
				exit(-1);
			}
			fl2sh_16bit(no_speech_samples, speech, buf, 1);
			drwav_write_pcm_frames(&dwav, no_speech_samples, buf);
			drwav_uninit(&dwav);
			free(buf);
		}
		else
		{
			write_samples(speech, no_speech_samples, out_filename);
			fclose(fp_speech);
		}




		free(speech);
		fprintf(fp_log, "\n");
}
