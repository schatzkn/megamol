/*
 * AstroParticleConverter.h
 *
 * Copyright (C) 2019 by VISUS (Universitaet Stuttgart)
 * Alle Rechte vorbehalten.
 */

#ifndef MEGAMOLCORE_ASTROPARTICLECONVERTER_H_INCLUDED
#define MEGAMOLCORE_ASTROPARTICLECONVERTER_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#    pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "mmcore/CalleeSlot.h"
#include "mmcore/CallerSlot.h"
#include "mmcore/Module.h"

#include "astro/AstroDataCall.h"
#include "mmcore/moldyn/MultiParticleDataCall.h"

namespace megamol {
namespace astro {

class AstroParticleConverter : public core::Module {
public:
    static const char* ClassName(void) { return "AstroParticleConverter"; }
    static const char* Description(void) {
        return "Converts data contained in a AstroDataCall to a MultiParticleDataCall";
    }
    static bool IsAvailable(void) { return true; }

    /** Ctor. */
    AstroParticleConverter(void);

    /** Dtor. */
    virtual ~AstroParticleConverter(void);

protected:
    virtual bool create(void);
    virtual void release(void);

private:
    bool getData(core::Call& call);
    bool getExtent(core::Call& call);

    core::CalleeSlot sphereDataSlot;
    core::CallerSlot astroDataSlot;
    size_t lastDataHash;
    size_t hashOffset;
	float colmin, colmax;
};

} // namespace astro
} // namespace megamol

#endif /*  */
