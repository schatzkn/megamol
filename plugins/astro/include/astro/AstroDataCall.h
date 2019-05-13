/*
 * AstroDataCall.h
 *
 * Copyright (C) 2019 by Universitaet Stuttgart (VISUS).
 * Alle Rechte vorbehalten.
 */

#ifndef MEGAMOLCORE_ASTRODATACALL_H_INCLUDED
#define MEGAMOLCORE_ASTRODATACALL_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#    pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include <glm/glm.hpp>
#include <memory>
#include "astro/astro.h"
#include "mmcore/AbstractGetData3DCall.h"
#include "mmcore/factories/CallAutoDescription.h"
#include "vislib/math/Cuboid.h"

namespace megamol {
namespace astro {

typedef std::shared_ptr<std::vector<glm::vec3>> vec3ArrayPtr;
typedef std::shared_ptr<std::vector<float>> floatArrayPtr;
typedef std::shared_ptr<std::vector<bool>> boolArrayPtr;
typedef std::shared_ptr<std::vector<int64_t>> idArrayPtr;

class ASTRO_API AstroDataCall : public core::AbstractGetData3DCall {
public:
    /**
     * Answer the name of the objects of this description.
     *
     * @return The name of the objects of this description.
     */
    static const char* ClassName(void) { return "AstroDataCall"; }


    /**
     * Gets a human readable description of the module.
     *
     * @return A human readable description of the module.
     */
    static const char* Description(void) { return "Call to get astronomical particle data."; }

    /** Index of the 'GetData' function */
    static const unsigned int CallForGetData;

    /** Index of the 'GetExtent' function */
    static const unsigned int CallForGetExtent;

    /** Ctor. */
    AstroDataCall(void);

    /** Dtor. */
    virtual ~AstroDataCall(void);

    /**
     * Answer the number of functions used for this call.
     *
     * @return The number of functions used for this call.
     */
    static unsigned int FunctionCount(void) { return 2; }

    /**
     * Answer the name of the function used for this call.
     *
     * @param idx The index of the function to return it's name.
     *
     * @return The name of the requested function.
     */
    static const char* FunctionName(unsigned int idx) {
        switch (idx) {
        case 0:
            return "getData";
        case 1:
            return "getExtent";
        }
        return "";
    }

    /**
     * Sets the position vector
     *
     * @param positionVec Pointer to the new position vector to be set
     */
    inline void SetPositions(vec3ArrayPtr& positionVec) { this->positions = positionVec; }

    /**
     * Retrieve the pointer to the vector storing the positions
     *
     * @return Pointer to the position array
     */
    inline const vec3ArrayPtr GetPositions(void) const { return this->positions; }

    /**
     * Sets the velocity vector
     *
     * @param velocityVec Pointer to the new velocity vector to be set
     */
    inline void SetVelocities(vec3ArrayPtr& velocityVec) { this->velocities = velocityVec; }

    /**
     * Retrieve the pointer to the vector storing the velocities
     *
     * @return Pointer to the velocity array
     */
    inline const vec3ArrayPtr GetVelocities(void) const { return this->velocities; }

    /**
     * Sets the temperature vector
     *
     * @param temparatureVec Pointer to the new temperature vector to be set
     */
    inline void SetTemperature(floatArrayPtr& temparatureVec) { this->temperatures = temparatureVec; }

    /**
     * Retrieve the pointer to the vector storing the temperature
     *
     * @return Pointer to the velocity array
     */
    inline const floatArrayPtr GetTemperature(void) const { return this->temperatures; }

    /**
     * Sets the mass vector
     *
     * @param massVec Pointer to the new mass vector to be set
     */
    inline void SetMass(floatArrayPtr& massVec) { this->masses = massVec; }

    /**
     * Retrieve the pointer to the vector storing the mass
     *
     * @return Pointer to the mass array
     */
    inline const floatArrayPtr GetMass(void) const { return this->masses; }

    /**
     * Sets the internal energy vector
     *
     * @param internalEnergyVec Pointer to the new internal energy vector to be set
     */
    inline void SetInternalEnergy(floatArrayPtr& internalEnergyVec) { this->internalEnergies = internalEnergyVec; }

    /**
     * Retrieve the pointer to the vector storing the internal energy
     *
     * @return Pointer to the internal energy array
     */
    inline const floatArrayPtr GetInternalEnergy(void) const { return this->internalEnergies; }

    /**
     * Sets the smoothing length vector
     *
     * @param smoothingLengthVec Pointer to the new smoothing length vector to be set
     */
    inline void SetSmoothingLength(floatArrayPtr& smoothingLengthVec) { this->smoothingLengths = smoothingLengthVec; }

    /**
     * Retrieve the pointer to the vector storing the smoothing length
     *
     * @return Pointer to the smoothing length array
     */
    inline const floatArrayPtr GetSmoothingLength(void) const { return this->smoothingLengths; }

    /**
     * Sets the molecular weight vector
     *
     * @param molecularWeightVec Pointer to the new molecular weight vector to be set
     */
    inline void SetMolecularWeights(floatArrayPtr& molecularWeightVec) { this->molecularWeights = molecularWeightVec; }

    /**
     * Retrieve the pointer to the vector storing the molecular weight
     *
     * @return Pointer to the molecular weight array
     */
    inline const floatArrayPtr GetMolecularWeights(void) const { return this->molecularWeights; }

    /**
     * Sets the density vector
     *
     * @param densityVec Pointer to the new density vector to be set
     */
    inline void SetDensity(floatArrayPtr& densityVec) { this->densities = densityVec; }

    /**
     * Retrieve the pointer to the vector storing the density
     *
     * @return Pointer to the density array
     */
    inline const floatArrayPtr GetDensity(void) const { return this->densities; }

    /**
     * Sets the gravitational potential vector
     *
     * @param gravitationalPotentialVec Pointer to the new gravitational potential vector to be set
     */
    inline void SetGravitationalPotential(floatArrayPtr& gravitationalPotentialVec) {
        this->gravitationalPotentials = gravitationalPotentialVec;
    }

    /**
     * Retrieve the pointer to the vector storing the gravitational potential
     *
     * @return Pointer to the gravitational potential array
     */
    inline const floatArrayPtr GetGravitationalPotential(void) const { return this->gravitationalPotentials; }

    /**
     * Sets the baryon flag vector
     *
     * @param isBaryonVec Pointer to the new baryon flag vector to be set
     */
    inline void SetIsBaryonFlags(boolArrayPtr& isBaryonVec) { this->isBaryonFlags = isBaryonVec; }

    /**
     * Retrieve the pointer to the vector storing the baryon flags
     * The content is true if the respecting particle is a baryon particle. It will contain 'false' if the particle is
     * dark matter.
     *
     * @return Pointer to the baryon flag array
     */
    inline const boolArrayPtr GetIsBaryonFlags(void) const { return this->isBaryonFlags; }

    /**
     * Sets the star flag vector
     *
     * @param isStarVec Pointer to the new star flag vector to be set
     */
    inline void SetIsStarFlags(boolArrayPtr& isStarVec) { this->isStarFlags = isStarVec; }

    /**
     * Retrieve the pointer to the vector storing the star flags
     *
     * @return Pointer to the star flag array
     */
    inline const boolArrayPtr GetIsStarFlags(void) const { return this->isStarFlags; }

    /**
     * Sets the wind flag vector
     *
     * @param isWindVec Pointer to the new wind flag vector to be set
     */
    inline void SetIsWindFlags(boolArrayPtr& isWindVec) { this->isWindFlags = isWindVec; }

    /**
     * Retrieve the pointer to the vector storing the wind flags
     *
     * @return Pointer to the wind flag array
     */
    inline const boolArrayPtr GetIsWindFlags(void) const { return this->isWindFlags; }

    /**
     * Sets the star forming gas flag vector
     *
     * @param isWindVec Pointer to the new star forming gas flag vector to be set
     */
    inline void SetIsStarFormingGasFlags(boolArrayPtr& isStarFormingGasVec) {
        this->isStarFormingGasFlags = isStarFormingGasVec;
    }

    /**
     * Retrieve the pointer to the vector storing the star forming gas flags
     *
     * @return Pointer to the star forming gas flag array
     */
    inline const boolArrayPtr GetIsStarFormingGasFlags(void) const { return this->isStarFormingGasFlags; }

    /**
     * Sets the AGN flag vector
     *
     * @param isWindVec Pointer to the new AGN flag vector to be set
     */
    inline void SetIsAGNFlags(boolArrayPtr& isAGNVec) { this->isAGNFlags = isAGNVec; }

    /**
     * Retrieve the pointer to the vector storing the AGN flags
     *
     * @return Pointer to the AGN flag array
     */
    inline const boolArrayPtr GetIsAGNFlags(void) const { return this->isAGNFlags; }

    /**
     * Sets the particle ID vector
     *
     * @param particleIDVec Pointer to the new particle ID vector to be set
     */
    inline void SetParticleIDs(idArrayPtr& particleIDVec) { this->particleIDs = particleIDVec; }

    /**
     * Retrieve the pointer to the vector storing the particle IDs
     *
     * @return Pointer to the particle ID array
     */
    inline const idArrayPtr GetParticleIDs(void) const { return this->particleIDs; }

    /**
     * Retrieve the number of particles stored in this call.
     * This will only result in a value greater 0 if the positions array is set.
     * All other arrays may be unset.
     *
     * @return The numbers of particles stored
     */
    inline size_t GetParticleCount(void) const {
        if (positions == nullptr) return 0;
        return positions->size();
    }

private:
    /** Pointer to the position array */
    vec3ArrayPtr positions;

    /** Pointer to the velocity array */
    vec3ArrayPtr velocities;

    /** Pointer to the temperature array */
    floatArrayPtr temperatures;

    /** Pointer to the mass array */
    floatArrayPtr masses;

    /** Pointer to the interal energy array */
    floatArrayPtr internalEnergies;

    /** Pointer to the smoothing length array */
    floatArrayPtr smoothingLengths;

    /** Pointer to the molecular weight array */
    floatArrayPtr molecularWeights;

    /** Pointer to the density array */
    floatArrayPtr densities;

    /** Pointer to the gravitational potential array */
    floatArrayPtr gravitationalPotentials;

    /** Pointer to the baryon flag array */
    boolArrayPtr isBaryonFlags;

    /** Pointer to the star flag array */
    boolArrayPtr isStarFlags;

    /** Pointer to the wind flag array */
    boolArrayPtr isWindFlags;

    /** Pointer to the star forming gas flag array */
    boolArrayPtr isStarFormingGasFlags;

    /** Pointer to the AGN flag array */
    boolArrayPtr isAGNFlags;

    /** Pointer to the particle ID array */
    idArrayPtr particleIDs;
};

/** Description class typedef */
typedef megamol::core::factories::CallAutoDescription<AstroDataCall> AstroDataCallDescription;

} // namespace astro
} // namespace megamol

#endif
