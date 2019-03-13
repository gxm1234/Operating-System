#ifndef STUDENTCONFIGURATION_H_
#define STUDENTCONFIGURATION_H_
/****************************************************************************
    StudentConfiguration.h
    Of all the files given to you at the start of this project, this
    is the ONLY one you should ever modify.

      4.30 Jan 2016           StudentConfiguration.h created
      4.50 Jan 2018           Automatically configure for underlying OS
****************************************************************************/
/****************************************************************************
    Choose one of the operating systems below
****************************************************************************/
/*#ifdef __unix
#define LINUX
#endif*/
#ifdef _WIN32 
#define WINDOWS
#endif
/*#ifdef __APPLE__
#define MAC
#endif*/

/*****************************************************************
    The next five defines have special meaning.  They allow the
    Z502 processor to report information about its state.  From
    this, you can find what the hardware thinks is going on.
    The information produced when this debugging is on is NOT
    something that should be handed in with the project.
    Change FALSE to TRUE to enable a feature.
******************************************************************/
#define         DO_DEVICE_DEBUG                 FALSE
#define         DO_MEMORY_DEBUG                 FALSE

//  These three are very useful for my debugging the hardware,
//  but are probably less useful for students.
#define         DEBUG_LOCKS                     FALSE
#define         DEBUG_CONDITION                 FALSE
#define         DEBUG_USER_THREADS              FALSE

#endif /* STUDENTCONFIGURATION_H_ */
