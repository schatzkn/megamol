#ifndef MMSTD_DATATOOLS_SIBSONINTERPOLATOR_H_INCLUDED
#define MMSTD_DATATOOLS_SIBSONINTERPOLATOR_H_INCLUDED
#pragma once

#include "mmcore/CalleeSlot.h"
#include "mmcore/CallerSlot.h"
#include "mmcore/Module.h"
#include "mmcore/misc/VolumetricDataCall.h"
#include "mmcore/moldyn/MultiParticleDataCall.h"
#include "mmcore/param/ParamSlot.h"

#include <array>
#include <limits>
#include <vector>

namespace megamol::stdplugin::datatools {

class SibsonInterpolator : public megamol::core::Module {
public:
    /** Return module class name */
    static const char* ClassName(void) {
        return "SibsonInterpolator";
    }

    /** Return module class description */
    static const char* Description(void) {
        return "Interpolates values of particles with a Sibson interpolation";
    }

    /** Module is always available */
    static bool IsAvailable(void) {
        return true;
    }

    /** Ctor */
    SibsonInterpolator(void);

    /** Dtor */
    virtual ~SibsonInterpolator(void);

protected:
    /** Lazy initialization of the module */
    virtual bool create(void);

    /** Resource release */
    virtual void release(void);

private:
    /**
     * Called when the data is requested by this module
     *
     * @param c The incoming call
     *
     * @return True on success
     */
    bool getDataCallback(megamol::core::Call& c);

    /**
     * Called when the extend information is requested by this module
     *
     * @param c The incoming call
     *
     * @return True on success
     */
    bool getExtentCallback(megamol::core::Call& c);

    bool dummyCallback(megamol::core::Call& c);

    bool createVolumeCPU(megamol::core::moldyn::MultiParticleDataCall* c2);

    inline bool anythingDirty() const {
        return this->aggregatorSlot.IsDirty() || this->xResSlot.IsDirty() || this->yResSlot.IsDirty() ||
               this->zResSlot.IsDirty() || this->cyclXSlot.IsDirty() || this->cyclYSlot.IsDirty() ||
               this->cyclZSlot.IsDirty();
    }

    inline void resetDirty() {
        this->aggregatorSlot.ResetDirty();
        this->xResSlot.ResetDirty();
        this->yResSlot.ResetDirty();
        this->zResSlot.ResetDirty();
        this->cyclXSlot.ResetDirty();
        this->cyclYSlot.ResetDirty();
        this->cyclZSlot.ResetDirty();
    }

    core::param::ParamSlot aggregatorSlot;

    core::param::ParamSlot xResSlot;
    core::param::ParamSlot yResSlot;
    core::param::ParamSlot zResSlot;

    core::param::ParamSlot cyclXSlot;
    core::param::ParamSlot cyclYSlot;
    core::param::ParamSlot cyclZSlot;

    size_t in_datahash = std::numeric_limits<size_t>::max();
    size_t datahash = 0;
    unsigned int time = 0;
    float maxDens = 0.0f;
    float minDens = std::numeric_limits<float>::max();
    bool has_data;

    /** The slot providing access to the manipulated data */
    megamol::core::CalleeSlot outDataSlot;

    /** The slot accessing the original data */
    megamol::core::CallerSlot inDataSlot;

    core::misc::VolumetricDataCall::Metadata metadata;
};

} // namespace megamol::stdplugin::datatools

#endif
