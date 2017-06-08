/*
 * MpiProvider.cpp
 *
 * Copyright (C) 2014 Visualisierungsinstitut der Universität Stuttgart.
 * Alle Rechte vorbehalten.
 */

#include "stdafx.h"
#include "mmcore/cluster/mpi/MpiProvider.h"

#ifndef WIN32
#include <cstdio>
#include <cstdlib>
#endif /* !_WIN32 */

#include "mmcore/param/IntParam.h"

#include "mmcore/cluster/mpi/MpiCall.h"

#include "vislib/assert.h"
#include "vislib/sys/CmdLineProvider.h"
#include "vislib/sys/Log.h"


/*
 * megamol::core::cluster::mpi::MpiProvider::IsAvailable
 */
bool megamol::core::cluster::mpi::MpiProvider::IsAvailable(void) {
#ifdef WITH_MPI
    return true;
#else /* WITH_MPI */
    return false;
#endif /* WITH_MPI */
}


/*
 * megamol::core::cluster::mpi::MpiProvider::MpiProvider
 */
megamol::core::cluster::mpi::MpiProvider::MpiProvider(void) : Base(),
        callProvideMpi("provideMpi", "Provides the MPI communicator etc."),
        paramNodeColour("nodeColour", "Specifies the node colour identifying the MegaMol instances.") {
#ifdef WITH_MPI        
    ASSERT(MpiProvider::comm.is_lock_free());
#endif /* WITH_MPI */

    this->callProvideMpi.SetCallback(MpiCall::ClassName(),
        MpiCall::FunctionName(MpiCall::IDX_PROVIDE_MPI),
        &MpiProvider::OnCallProvideMpi);
    this->MakeSlotAvailable(&this->callProvideMpi);

    this->paramNodeColour << new param::IntParam(0, 0);
    this->MakeSlotAvailable(&this->paramNodeColour);
}


/*
 * megamol::core::cluster::mpi::MpiProvider::~MpiProvider
 */
megamol::core::cluster::mpi::MpiProvider::~MpiProvider(void) {
    this->Release();
}


/*
 * megamol::core::cluster::mpi::MpiProvider::create
 */
bool megamol::core::cluster::mpi::MpiProvider::create(void) {
    ++MpiProvider::activeInstances;
    return true;
}


/*
 * megamol::core::cluster::mpi::MpiProvider::OnCallProvideMpi
 */
bool megamol::core::cluster::mpi::MpiProvider::OnCallProvideMpi(Call& call) {
#ifdef WITH_MPI
    auto colour = this->paramNodeColour.Param<param::IntParam>()->Value();

    try {
        if (MpiProvider::initialiseMpi(colour)) {
            /* MPI was initialised now or before. */
            ASSERT(MpiProvider::comm  != MPI_COMM_NULL);
            auto& c = dynamic_cast<MpiCall&>(call);
            c.SetComm(MpiProvider::comm);
            return true;

        } else {
            /* Initialisation of MPI failed. */
            return false;
        }

    } catch (...) {
        return false;
    }

#else /* WITH_MPI */
    return false;
#endif /* WITH_MPI */
}


/*
 * megamol::core::cluster::mpi::MpiProvider::release
 */
void megamol::core::cluster::mpi::MpiProvider::release(void) {
    using vislib::sys::Log;

    ASSERT(MpiProvider::activeInstances.load() > 0);
    if (--MpiProvider::activeInstances == 0) {
#ifdef WITH_MPI
        auto comm = MpiProvider::comm.exchange(MPI_COMM_NULL);
        if (comm != MPI_COMM_NULL) {
            Log::DefaultLog.WriteInfo("Releasing MPI communicator ...");
            ::MPI_Comm_free(&comm);
        }

        if (MpiProvider::isMpiOwner) {
            Log::DefaultLog.WriteInfo("Finalising MPI ...");
            ::MPI_Finalize();
        }

        MpiProvider::activeNodeColour.store(MPI_UNDEFINED);
#endif /* WITH_MPI */
    }
    ASSERT(MpiProvider::activeInstances.load() >= 0);
}


/*
 * megamol::core::cluster::mpi::MpiProvider::getCommandLine
 */
vislib::StringA megamol::core::cluster::mpi::MpiProvider::getCommandLine(void) {
    vislib::StringA retval;

#ifdef WIN32
    retval = ::GetCommandLineA();
#else /* _WIN32 */
    char *arg = nullptr;
    size_t size = 0;

    auto fp = ::fopen("/proc/self/cmdline", "rb");
    if (fp != nullptr) {
        while (::getdelim(&arg, &size, 0, fp) != -1) {
            retval.Append(arg, size);
            retval.Append(" ");
        }
        ::free(arg);
        ::fclose(fp);
    }
#endif /* _WIN32 */

    vislib::sys::Log::DefaultLog.WriteInfo("Command line used for MPI "
        "initialisation is \"%s\".", retval.PeekBuffer());
    return retval;
}


/*
 * megamol::core::cluster::mpi::MpiProvider::initialiseMpi
 */
bool megamol::core::cluster::mpi::MpiProvider::initialiseMpi(const int colour) {
    using vislib::sys::Log;

#ifdef WITH_MPI
    int expectedColour = MPI_UNDEFINED;

    if (MpiProvider::activeNodeColour.compare_exchange_strong(expectedColour,
            colour)) {
        /* Initialisation has not yet been performed, so do it now. */
        ASSERT(MpiProvider::activeNodeColour.load() == colour);
        ASSERT(MpiProvider::comm.load() == MPI_COMM_NULL);

        MPI_Comm comm = MPI_COMM_NULL;
        int isInitialised = 0;
        int rank = 0;

        /* Determine whether we "own" MPI, and if so, initialise it. */
        // TODO: Check status?
        ::MPI_Initialized(&isInitialised);
        MpiProvider::isMpiOwner = (isInitialised == 0);

        if (MpiProvider::isMpiOwner) {
            vislib::sys::CmdLineProviderA cl(MpiProvider::getCommandLine());
            auto argc = cl.ArgC();
            auto argv = cl.ArgV();

            Log::DefaultLog.WriteInfo("Initialising MPI ...");
            auto status =::MPI_Init(&argc, &argv);
            if (status != MPI_SUCCESS) {
                Log::DefaultLog.WriteError("MPI_Init failed with error code "
                    "%d. Future calls might retry the operation.", status);
                MpiProvider::activeNodeColour.store(MPI_UNDEFINED);
                return false;
            }
        } else {
            Log::DefaultLog.WriteInfo("MPI has already been initialised before "
                "MpiProvider::initialiseMpi was called.");
        } /* if (MpiProvider::isMpiOwner) */
        ASSERT((isInitialised != 0) || MpiProvider::isMpiOwner);

        /* Now, perform the node colouring and obtain the communicator. */
        Log::DefaultLog.WriteInfo("Performing node colouring with colour "
            "%d ...", colour);
        // TODO: Check status?
        ::MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        ::MPI_Comm_split(MPI_COMM_WORLD, colour, rank, &comm);
        MpiProvider::comm.store(comm);

    } else {
        /* Initialisation has already been performed, check reconfiguration. */

        if (expectedColour != colour) {
            Log::DefaultLog.WriteWarn("MPI has already been initialised before "
                "and node colouring was done using %d. The current request "
                "uses the node colour %d, which will be ignored. Please ensure "
                "that the node colour does not change after MPI has been "
                "initialised the first time.", expectedColour, colour);
        }
    }
    /*
     * At this point, MPI was already initialised or some one else is currently
     * initialising it. As settings the communicator indicates that the critical
     * section is left, wait for this condition before returning.
     */
    ASSERT(MpiProvider::activeNodeColour.load() != MPI_UNDEFINED);
    while (MpiProvider::comm.load() == MPI_COMM_NULL);
    ASSERT(MpiProvider::comm.load() != MPI_COMM_NULL);

    return true;

#else /* WITH_MPI */
    return false;
#endif /* WITH_MPI */
}


/*
 * megamol::core::cluster::mpi::MpiProvider::activeInstances
 */
std::atomic<int> megamol::core::cluster::mpi::MpiProvider::activeInstances(0);


#ifdef WITH_MPI
/*
 * megamol::core::cluster::mpi::MpiProvider::comm
 */
std::atomic<MPI_Comm> megamol::core::cluster::mpi::MpiProvider::comm(
    MPI_COMM_NULL);
#endif /* WITH_MPI */


/*
 * megamol::core::cluster::mpi::MpiProvider::activeNodeColour
 */
std::atomic<int> megamol::core::cluster::mpi::MpiProvider::activeNodeColour(
#ifdef WITH_MPI
    MPI_UNDEFINED);
#else /* WITH_MPI */
    -1);
#endif /* WITH_MPI */


/*
 * megamol::core::cluster::mpi::MpiProvider::isMpiOwner
 */
bool megamol::core::cluster::mpi::MpiProvider::isMpiOwner = false;