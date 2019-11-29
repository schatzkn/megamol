/*
 * FEMMaterialDataSource.h
 *
 * Copyright (C) 2019 by Universitaet Stuttgart (VISUS).
 * All rights reserved.
 */

#ifndef FEM_MATERIAL_DATA_SOURCE_H_INCLUDED
#define FEM_MATERIAL_DATA_SOURCE_H_INCLUDED
#if (defined(_MSC_VER) && (_MSC_VER > 1000))
#    pragma once
#endif /* (defined(_MSC_VER) && (_MSC_VER > 1000)) */

#include "mmcore/CallerSlot.h"
#include "mmcore/param/ParamSlot.h"

#include "mesh/AbstractGPUMaterialDataSource.h"

namespace megamol {
namespace archvis {

class FEMMaterialDataSource : public mesh::AbstractGPUMaterialDataSource {
public:
    /**
     * Answer the name of this module.
     *
     * @return The name of this module.
     */
    static const char* ClassName(void) { return "FEMMaterialDataSource"; }

    /**
     * Answer a human readable description of this module.
     *
     * @return A human readable description of this module.
     */
    static const char* Description(void) {
        return "FEM material data source for loading a BTF Shader file that can be connected to a transfer function";
    }

    /**
     * Answers whether this module is available on the current system.
     *
     * @return 'true' if the module is available, 'false' otherwise.
     */
    static bool IsAvailable(void) { return true; }


    FEMMaterialDataSource();
    ~FEMMaterialDataSource();

protected:
    virtual bool create();

    virtual bool getDataCallback(core::Call& caller);

    virtual bool getMetaDataCallback(core::Call& caller);

private:
    /** The btf file name */
    core::param::ParamSlot m_btf_filename_slot;

    /** The call for Transfer function */
    core::CallerSlot m_transferFunction_slot;

    uint32_t tfVersion = -1;
};

} // namespace archvis
} // namespace megamol


#endif // !FEM_MATERIAL_DATA_SOURCE_H_INCLUDED
