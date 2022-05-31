#define BUFFER_LENGTH 44100
#define INPUT_BUFFER_LENGTH 1000

//RemoveLaughs buffering variables
int InputBuffer[INPUT_BUFFER_LENGTH]; //1000 length input buffer

int CurrentInputWritePos = 0;

unsigned long WorkBuffer[BUFFER_LENGTH];
int OutputBuffer[BUFFER_LENGTH]; //44100 length circular output buffer (assumes 16-bit sound data)
int CurrentReadPos = 0;
int CurrentWritePos = 0;

//optimizations
int WorkBufferValue = 0;
int WorkValue = 0;
int FilterIndex = 999;

//state variables
int LaughDetectionState = 0; //0 - no laugh, 1 - possible laugh, 2 - definite laugh
int DetectedSamples = 0;

//samples per second
const int m_SamplesPerSecond = 44100;

extern int HammingWindowedLPF[];

void RemoveLaughs(PBYTE pb, int cb);

////////////////////////

//
// RemoveLaughs
//
// PBYTE pb - pointer to the input/output data, here it is assumed to be a 16 bit PCM
//            sample, rather than a byte like the name would indicate.
// int cb   - number of bytes in the stream pointed to by pb, note that this is in
//            bytes even though we want words (so we add 2 instead of 1 for the loop).
//
void CGargle::RemoveLaughs(PBYTE pb, int cb)
{
    while (cb > 0)
    {
        // 16 bit sound uses 16 bits properly (0 means 0)
        // We still clip paranoically

        //buffer the incoming data
        short int *psi = (short int *)pb;
        InputBuffer[CurrentInputWritePos] = (*psi < 0 ) ? -*psi : *psi;
        OutputBuffer[CurrentWritePos] = *psi;

        //run the transformation on OutputBuffer
        WorkValue += (HammingWindowedLPF[FilterIndex] * InputBuffer[CurrentInputWritePos]) >> 16;
        FilterIndex--;
        if(FilterIndex < 0)
        {
            FilterIndex = 999;
            WorkBufferValue = WorkValue;
            WorkValue = 0;
        }

        int i = WorkBufferValue;
        WorkBuffer[CurrentWritePos] = (unsigned long)((long)i * (long)i);

#define HEIGHT_THRESH 22e5 //25e5
#define WIDTH_THRESH  0.8
#define CUTOFF_PERCENT 0.5

        //WorkBuffer now contains the filtered and squared input
        switch(LaughDetectionState)
        {
            default:
            case 0:
                {
                    if(WorkBuffer[CurrentWritePos] > HEIGHT_THRESH )
                    {
                        LaughDetectionState = 1;
                        DetectedSamples = 0;
                    }

                } break;

            case 1:
                {
                    if(WorkBuffer[CurrentWritePos] < (CUTOFF_PERCENT * HEIGHT_THRESH))
                    {
                        DetectedSamples = 0;
                        LaughDetectionState = 0;
                    }
                    else
                    {
                        DetectedSamples++;
                    }

                    if(DetectedSamples > (WIDTH_THRESH * m_SamplesPerSec))
                    {
                        LaughDetectionState = 2;

                        //go back WIDTH_THRESH*m_SamplesPerSec in time, killing the signal output
                        //and then continue killing the signal output in state 2
                        int pos = 0;
                        for(int count = 0; count < (WIDTH_THRESH * m_SamplesPerSec); count++)
                        {
                            pos = CurrentWritePos - count;
                            while(pos < 0) {pos+=BUFFER_LENGTH;}

                            OutputBuffer[pos] = 0;
                        }
                    }

                } break;

            case 2:
                {
                    if(WorkBuffer[CurrentWritePos] < (CUTOFF_PERCENT * HEIGHT_THRESH))
                    {
                        DetectedSamples = 0;
                        LaughDetectionState = 0;
                    }
                    else
                    {
                        DetectedSamples++;
                    }

                    //continue suppressing the laughter until the detection terminates
                    OutputBuffer[CurrentWritePos] = 0;

                } break;

        //clip and write the output
        i = OutputBuffer[CurrentReadPos];
        if (i>32767)  i = 32767;
        if (i<-32768) i = -32768;
        *psi = (short)i;

        //increase the read and write indices
        CurrentInputWritePos = (CurrentInputWritePos + 1) % INPUT_BUFFER_LENGTH;
        CurrentWritePos = (CurrentWritePos + 1) % BUFFER_LENGTH;
        CurrentReadPos  = (CurrentReadPos  + 1) % BUFFER_LENGTH;

        pb += 2;
        cb -= 2;
    }

} // RemoveLaughs
