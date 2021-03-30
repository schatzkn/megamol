/*
 * SibsonInterpolator.h
 *
 * Copyright (C) 2021 by MegaMol team
 * Alle Rechte vorbehalten.
 */
#include "stdafx.h"
#include "SibsonInterpolator.h"

#include "mmcore/param/BoolParam.h"
#include "mmcore/param/EnumParam.h"
#include "mmcore/param/FloatParam.h"
#include "mmcore/param/IntParam.h"

#include "mmcore/utility/log/Log.h"

using namespace megamol;
using namespace megamol::stdplugin;
using namespace megamol::stdplugin::datatools;

bool SibsonInterpolator::create(void) {
    return true;
}

void SibsonInterpolator::release(void) {
    delete[] this->metadata.MinValues;
    delete[] this->metadata.MaxValues;
    delete[] this->metadata.SliceDists[0];
    delete[] this->metadata.SliceDists[1];
    delete[] this->metadata.SliceDists[2];
}

SibsonInterpolator::SibsonInterpolator(void)
        : aggregatorSlot("aggregator", "algorithm for the aggregation")
        , xResSlot("sizex", "The size of the volume in numbers of voxels")
        , yResSlot("sizey", "The size of the volume in numbers of voxels")
        , zResSlot("sizez", "The size of the volume in numbers of voxels")
        , cyclXSlot("cyclX", "Considers cyclic boundary conditions in X direction")
        , cyclYSlot("cyclY", "Considers cyclic boundary conditions in Y direction")
        , cyclZSlot("cyclZ", "Considers cyclic boundary conditions in Z direction")
        , datahash(0)
        , time(std::numeric_limits<unsigned int>::max())
        , has_data(false)
        , outDataSlot("outData", "Provides a density volume for the particles")
        , inDataSlot("inData", "Takes the particle data") {

    auto* ep = new core::param::EnumParam(0);
    ep->SetTypePair(0, "PosToSingleCell_Volume");
    ep->SetTypePair(1, "IColToSingleCell_Volume");
    ep->SetTypePair(2, "IVecToSingleCell_Volume");
    this->aggregatorSlot << ep;
    this->MakeSlotAvailable(&this->aggregatorSlot);

    this->outDataSlot.SetCallback(core::misc::VolumetricDataCall::ClassName(),
        core::misc::VolumetricDataCall::FunctionName(core::misc::VolumetricDataCall::IDX_GET_DATA),
        &SibsonInterpolator::getDataCallback);
    this->outDataSlot.SetCallback(core::misc::VolumetricDataCall::ClassName(),
        core::misc::VolumetricDataCall::FunctionName(core::misc::VolumetricDataCall::IDX_GET_EXTENTS),
        &SibsonInterpolator::getExtentCallback);
    this->outDataSlot.SetCallback(core::misc::VolumetricDataCall::ClassName(),
        core::misc::VolumetricDataCall::FunctionName(core::misc::VolumetricDataCall::IDX_GET_METADATA),
        &SibsonInterpolator::getExtentCallback);
    this->outDataSlot.SetCallback(core::misc::VolumetricDataCall::ClassName(),
        core::misc::VolumetricDataCall::FunctionName(core::misc::VolumetricDataCall::IDX_START_ASYNC),
        &SibsonInterpolator::dummyCallback);
    this->outDataSlot.SetCallback(core::misc::VolumetricDataCall::ClassName(),
        core::misc::VolumetricDataCall::FunctionName(core::misc::VolumetricDataCall::IDX_STOP_ASYNC),
        &SibsonInterpolator::dummyCallback);
    this->outDataSlot.SetCallback(core::misc::VolumetricDataCall::ClassName(),
        core::misc::VolumetricDataCall::FunctionName(core::misc::VolumetricDataCall::IDX_TRY_GET_DATA),
        &SibsonInterpolator::dummyCallback);
    this->MakeSlotAvailable(&this->outDataSlot);

    this->inDataSlot.SetCompatibleCall<megamol::core::moldyn::MultiParticleDataCallDescription>();
    this->MakeSlotAvailable(&this->inDataSlot);

    this->xResSlot << new core::param::IntParam(16);
    this->MakeSlotAvailable(&this->xResSlot);
    this->yResSlot << new core::param::IntParam(16);
    this->MakeSlotAvailable(&this->yResSlot);
    this->zResSlot << new core::param::IntParam(16);
    this->MakeSlotAvailable(&this->zResSlot);

    this->cyclXSlot.SetParameter(new core::param::BoolParam(true));
    this->MakeSlotAvailable(&this->cyclXSlot);
    this->cyclYSlot.SetParameter(new core::param::BoolParam(true));
    this->MakeSlotAvailable(&this->cyclYSlot);
    this->cyclZSlot.SetParameter(new core::param::BoolParam(true));
    this->MakeSlotAvailable(&this->cyclZSlot);
}

SibsonInterpolator::~SibsonInterpolator(void) {
    this->Release();
}

bool SibsonInterpolator::getExtentCallback(megamol::core::Call& c) {
    using megamol::core::moldyn::MultiParticleDataCall;

    auto* out = dynamic_cast<core::misc::VolumetricDataCall*>(&c);

    auto* inMpdc = this->inDataSlot.CallAs<MultiParticleDataCall>();
    if (inMpdc == nullptr)
        return false;

    auto frameID = out != nullptr ? out->FrameID() : 0;
    // vislib::sys::Log::DefaultLog.WriteInfo(L"ParticleToDensity requests frame %u.", frameID);
    inMpdc->SetFrameID(frameID, true);
    if (!(*inMpdc)(1)) {
        megamol::core::utility::log::Log::DefaultLog.WriteError(
            "SibsonInterpolator: could not get current frame extents (%u)", time - 1);
        return false;
    }

    if (out != nullptr) {
        out->AccessBoundingBoxes().SetObjectSpaceBBox(inMpdc->GetBoundingBoxes().ObjectSpaceBBox());
        out->AccessBoundingBoxes().SetObjectSpaceClipBox(inMpdc->GetBoundingBoxes().ObjectSpaceClipBox());
        out->AccessBoundingBoxes().MakeScaledWorld(1.0f);
        out->SetFrameCount(inMpdc->FrameCount());
    }

    return true;
}

// TODO functions

bool SibsonInterpolator::dummyCallback(megamol::core::Call& c) {
    return true;
}
