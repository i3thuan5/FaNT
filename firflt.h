/*
  ============================================================================
   File: FIRFLT.H                                v.2.0 -  20.Apr.94 (18:00:00)
  ============================================================================

             UGST/CCITT HIGH QUALITY FIR UP/DOWN-SAMPLING FILTER

                          GLOBAL FUNCTION PROTOTYPES

   History:
   28.Feb.92	v1.0	First version <hf@pkinbg.uucp>
   20.Apr.94	v2.0	Added smart prototypes & new functions for HQFLT v.2.0
   30.Sep.94    v2.1    Changed name of module to FIR and split different 
                        filters to different files, to ease expansion.
  ============================================================================
*/
 
#ifndef FIRFLT_FIRstruct_defined
#define FIRFLT_FIRstruct_defined 210


/* DEFINITION FOR SMART PROTOTYPES */
#ifndef ARGS
#if defined(MSDOS) || defined(__MSDOS__) || defined(__STDC__) || defined(VMS)
#define ARGS(x) x
#else /* Unix: no parameters in prototype! */
#define ARGS(x) ()
#endif
#endif

/* 
 * ..... State variable structure for FIR filtering ..... 
 */

typedef struct {
        long  lenh0;                    /* number of FIR coefficients        */
        long  dwn_up;                   /* down sampling factor              */
        long  k0;                       /* start index in next segment       */
                                        /* (needed in segmentwise filtering) */
        float *h0;                      /* pointer to array with FIR coeff.  */
        float *T;                       /* pointer to delay line             */
        char  hswitch;                  /* switch to FIR-kernel              */
} SCD_FIR;


/* 
 * ..... Global function prototypes ..... 
 */

long hq_kernel ARGS((long lseg, float *x_ptr, SCD_FIR *fir_ptr, float *y_ptr));
SCD_FIR *hq_down_2_to_1_init ARGS((void));
SCD_FIR *hq_up_1_to_2_init ARGS((void));
SCD_FIR *hq_down_3_to_1_init ARGS((void));
SCD_FIR *hq_up_1_to_3_init ARGS((void));
SCD_FIR *irs_8khz_init ARGS((void));
SCD_FIR *irs_16khz_init ARGS((void));
SCD_FIR *mod_irs_16khz_init ARGS((void));
SCD_FIR *mod_irs_48khz_init ARGS((void));
SCD_FIR *tia_irs_8khz_init ARGS((void));
SCD_FIR *ht_irs_16khz_init ARGS((void));
SCD_FIR *delta_sm_16khz_init ARGS((void));
SCD_FIR *linear_phase_pb_2_to_1_init ARGS((void));
SCD_FIR *linear_phase_pb_1_to_2_init ARGS((void));
SCD_FIR *psophometric_8khz_init ARGS((void));
SCD_FIR *p341_16khz_init ARGS((void));
void hq_free ARGS((SCD_FIR *fir_ptr));
void hq_reset ARGS((SCD_FIR *fir_ptr));


/* Aurora addition */
SCD_FIR *fir_hp_8khz_init ARGS((void));


#endif /* FIRFLT_FIRstruct_defined */

/* *************************** End of firflt.h *************************** */
