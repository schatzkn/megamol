#include "stdafx.h"

#include "FlagCall.h"
#include "ScatterplotMatrixRenderer2D.h"

#include "mmcore/CoreInstance.h"
#include "mmcore/param/BoolParam.h"
#include "mmcore/param/EnumParam.h"
#include "mmcore/param/FlexEnumParam.h"
#include "mmcore/param/FloatParam.h"
#include "mmcore/param/IntParam.h"
#include "mmcore/param/StringParam.h"
#include "mmcore/utility/ColourParser.h"
#include "mmcore/utility/ResourceWrapper.h"
#include "mmcore/view/CallGetTransferFunction.h"

#include "vislib/math/ShallowMatrix.h"

#include <sstream>

using namespace megamol;
using namespace megamol::infovis;
using namespace megamol::stdplugin::datatools;

using vislib::sys::Log;

const GLuint SSBOBindingPoint = 2;

ScatterplotMatrixRenderer2D::ScatterplotMatrixRenderer2D()
    : core::view::Renderer2DModule()
    , floatTableInSlot("ftIn", "Float table input")
    , transferFunctionInSlot("tfIn", "Transfer function input")
    , flagStorageInSlot("fsIn", "Flag storage input")
    , columnsParam("columns", "Sets which columns should be displayed and in which order (empty means all)")
    , colorSelectorParam("colorSelector", "Sets a color column")
    , labelSelectorParam("labelSelector", "Sets a label column")
    , geometryTypeParam("geometryType", "Geometry type to map data to")
    , kernelWidthParam("kernelWidth", "Kernel width of the geometry, i.e., point size or line width")
    , axisColorParam("axisColor", "Color of axis")
    , axisWidthParam("axisWidth", "Line width for the axis")
    , axisTicksXParam("axisXTicks", "Number of ticks on the X axis")
    , axisTicksYParam("axisYTicks", "Number of ticks on the Y axis")
    , scaleXParam("scaleX", "Aspect ratio scaling x axis length")
    , scaleYParam("scaleY", "Set the scaling of y axis")
    , alphaScalingParam("alphaScaling", "Scaling factor for overall alpha")
    , attenuateSubpixelParam("attenuateSubpixel", "Attenuate alpha of points that should have subpixel size")
    , mouse({0, 0, false, false}) {
    this->floatTableInSlot.SetCompatibleCall<floattable::CallFloatTableDataDescription>();
    this->MakeSlotAvailable(&this->floatTableInSlot);

    this->transferFunctionInSlot.SetCompatibleCall<core::view::CallGetTransferFunctionDescription>();
    this->MakeSlotAvailable(&this->transferFunctionInSlot);

    this->flagStorageInSlot.SetCompatibleCall<FlagCallDescription>();
    this->MakeSlotAvailable(&this->flagStorageInSlot);

    this->columnsParam << new core::param::StringParam("");
    this->MakeSlotAvailable(&this->columnsParam);

    this->colorSelectorParam << new core::param::FlexEnumParam("undef");
    this->MakeSlotAvailable(&this->colorSelectorParam);

    this->labelSelectorParam << new core::param::FlexEnumParam("undef");
    this->MakeSlotAvailable(&this->labelSelectorParam);

    core::param::EnumParam* geometryTypes = new core::param::EnumParam(0);
    geometryTypes->SetTypePair(GEOMETRY_TYPE_POINT, "Point");
    geometryTypes->SetTypePair(GEOMETRY_TYPE_LINE, "Line");
    this->geometryTypeParam << geometryTypes;
    this->MakeSlotAvailable(&this->geometryTypeParam);

    this->kernelWidthParam << new core::param::FloatParam(1.0f, 0.0001f);
    this->MakeSlotAvailable(&this->kernelWidthParam);

    this->axisColorParam << new core::param::StringParam("white");
    this->MakeSlotAvailable(&this->axisColorParam);

    this->axisWidthParam << new core::param::FloatParam(1.0f, 0.0001f, 10000.0f);
    this->MakeSlotAvailable(&this->axisWidthParam);

    this->axisTicksXParam << new core::param::IntParam(4, 3, 100);
    this->MakeSlotAvailable(&this->axisTicksXParam);

    this->axisTicksYParam << new core::param::IntParam(4, 3, 100);
    this->MakeSlotAvailable(&this->axisTicksYParam);

    this->scaleXParam << new core::param::FloatParam(1.0f, 0.0f);
    this->MakeSlotAvailable(&this->scaleXParam);

    this->scaleYParam << new core::param::FloatParam(1.0f, 0.0001f);
    this->MakeSlotAvailable(&this->scaleYParam);

    this->alphaScalingParam << new core::param::FloatParam(1.0f, 0.0f);
    this->MakeSlotAvailable(&this->alphaScalingParam);

    this->attenuateSubpixelParam << new core::param::BoolParam(false);
    this->MakeSlotAvailable(&this->attenuateSubpixelParam);
}

ScatterplotMatrixRenderer2D::~ScatterplotMatrixRenderer2D() { this->Release(); }


bool ScatterplotMatrixRenderer2D::create(void) {
    if (!makeProgram("::splom", this->shader)) return false;

    return true;
}

void ScatterplotMatrixRenderer2D::release(void) {}

bool ScatterplotMatrixRenderer2D::MouseEvent(float x, float y, core::view::MouseFlags flags) {
    bool leftDown = (flags & core::view::MOUSEFLAG_BUTTON_LEFT_DOWN) != 0;
    bool rightDown = (flags & core::view::MOUSEFLAG_BUTTON_RIGHT_DOWN) != 0;
    bool rightChanged = (flags & core::view::MOUSEFLAG_BUTTON_RIGHT_CHANGED) != 0;

    this->mouse.x = x;
    this->mouse.y = y;
    this->mouse.selects = leftDown;
    this->mouse.inspects = rightDown && rightChanged;

    if (this->mouse.selects || this->mouse.inspects) {
        // TODO: Some hit testing might be nice here when clicking transparent areas.
        return true;
    }

    return false;
}

bool ScatterplotMatrixRenderer2D::Render(core::view::CallRender2D& call) {
    try {
        if (!this->validateData()) return false;

        this->drawYAxis();
        this->drawXAxis();

        auto geometryType = this->geometryTypeParam.Param<core::param::EnumParam>()->Value();
        switch (geometryType) {
        case GEOMETRY_TYPE_POINT:
            this->drawPoints();
            break;
        case GEOMETRY_TYPE_LINE:
            this->drawLines();
            break;
        }
    } catch (...) {
        return false;
    }

    return true;
}

bool ScatterplotMatrixRenderer2D::GetExtents(core::view::CallRender2D& call) {
    call.SetBoundingBox(0.0f, 0.0f, call.GetWidth(), call.GetHeight());
    return true;
}

bool ScatterplotMatrixRenderer2D::makeProgram(std::string prefix, vislib::graphics::gl::GLSLShader& program) {
    vislib::graphics::gl::ShaderSource vert, frag;

    vislib::StringA vertname((prefix + "::vert").c_str());
    vislib::StringA fragname((prefix + "::frag").c_str());
    vislib::StringA pref(prefix.c_str());

    if (!this->instance()->ShaderSourceFactory().MakeShaderSource(vertname, vert)) return false;
    if (!this->instance()->ShaderSourceFactory().MakeShaderSource(fragname, frag)) return false;

    try {
        if (!program.Create(vert.Code(), vert.Count(), frag.Code(), frag.Count())) {
            vislib::sys::Log::DefaultLog.WriteMsg(
                vislib::sys::Log::LEVEL_ERROR, "Unable to compile %s: Unknown error\n", pref.PeekBuffer());
            return false;
        }
    } catch (vislib::graphics::gl::AbstractOpenGLShader::CompileException ce) {
        vislib::sys::Log::DefaultLog.WriteMsg(vislib::sys::Log::LEVEL_ERROR, "Unable to compile %s (@%s): %s\n",
            pref.PeekBuffer(),
            vislib::graphics::gl::AbstractOpenGLShader::CompileException::CompileActionName(ce.FailedAction()),
            ce.GetMsgA());
        return false;
    } catch (vislib::Exception e) {
        vislib::sys::Log::DefaultLog.WriteMsg(
            vislib::sys::Log::LEVEL_ERROR, "Unable to compile %s: %s\n", pref.PeekBuffer(), e.GetMsgA());
        return false;
    } catch (...) {
        vislib::sys::Log::DefaultLog.WriteMsg(
            vislib::sys::Log::LEVEL_ERROR, "Unable to compile %s: Unknown exception\n", pref.PeekBuffer());
        return false;
    }

    return true;
}

bool ScatterplotMatrixRenderer2D::isDirty(void) const {
    return this->columnsParam.IsDirty() || this->colorSelectorParam.IsDirty() || this->labelSelectorParam.IsDirty();
}

void ScatterplotMatrixRenderer2D::resetDirty(void) {
    this->columnsParam.ResetDirty();
    this->colorSelectorParam.ResetDirty();
    this->labelSelectorParam.ResetDirty();
}

bool ScatterplotMatrixRenderer2D::validateData(void) {
    this->floatTable = this->floatTableInSlot.CallAs<floattable::CallFloatTableData>();
    if (this->floatTable == nullptr || !(*(this->floatTable))(0)) return false;

    if (this->dataHash == this->floatTable->DataHash() && !isDirty()) return true;

    auto columnInfos = this->floatTable->GetColumnsInfos();
    const size_t colCount = this->floatTable->GetColumnsCount();

    if (this->dataHash != this->floatTable->DataHash()) {
        // Update dynamic parameters.
        this->colorSelectorParam.Param<core::param::FlexEnumParam>()->ClearValues();
        this->labelSelectorParam.Param<core::param::FlexEnumParam>()->ClearValues();
        for (size_t i = 0; i < colCount; i++) {
            this->colorSelectorParam.Param<core::param::FlexEnumParam>()->AddValue(columnInfos[i].Name());
            this->labelSelectorParam.Param<core::param::FlexEnumParam>()->AddValue(columnInfos[i].Name());
        }
    }

    // Resolve selectors.
    auto nameToIndex = [&](const std::string& name, size_t defaultIdx) -> size_t {
        for (size_t i = 0; i < colCount; i++) {
            if (columnInfos[i].Name().compare(name) == 0) {
                return i;
            }
        }
        return defaultIdx;
    };
    map.colorIdx = nameToIndex(this->colorSelectorParam.Param<core::param::FlexEnumParam>()->Value(), 0);
    map.labelIdx = nameToIndex(this->labelSelectorParam.Param<core::param::FlexEnumParam>()->Value(), 0);

    updateColumns();

    this->dataHash = this->floatTable->DataHash();
    this->resetDirty();

    return true;
}

void ScatterplotMatrixRenderer2D::updateColumns(void) {
    auto columnCount = this->floatTable->GetColumnsCount();
    const float size = 10.0f; // XXX: make this guy configurable, i.e., for zooming.

    plots.clear();
    for (GLuint y = 0; y < columnCount; ++y) {
        for (GLuint x = 0; x < y; ++x) {
            plots.push_back({x, y, x * size, y * size, size, size});
        }
    }

    const GLuint numChunks = this->plotSSBO.SetDataWithSize(
        plots.data(), sizeof(PlotInfo), sizeof(PlotInfo), plots.size(), 1, plots.size() * sizeof(PlotInfo));

    GLuint numItems, sync;
    GLsizeiptr dstOffset, dstLength;
    rowSSBO.UploadChunk(0, numItems, sync, dstOffset, dstLength);
    rowSSBO.SignalCompletion(sync);
}

void ScatterplotMatrixRenderer2D::drawPoints(void) {
    auto xScale = this->scaleXParam.Param<core::param::FloatParam>()->Value();
    auto yScale = this->scaleYParam.Param<core::param::FloatParam>()->Value();

    auto columnInfos = this->floatTable->GetColumnsInfos();
    auto columnCount = this->floatTable->GetColumnsCount();

    GLfloat viewport[4];
    glGetFloatv(GL_VIEWPORT, viewport);

    // Blending.
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    // Point sprites.
    glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glEnable(GL_POINT_SPRITE);
    glPointSize(std::max(viewport[2], viewport[3]));

    // XXX: maybe we should move this before first usage?
    viewport[2] = 2.0f / std::max(1.0f, viewport[2]);
    viewport[3] = 2.0f / std::max(1.0f, viewport[3]);

    // this is the apex of suck and must die
    GLfloat modelViewMatrix_column[16];
    glGetFloatv(GL_MODELVIEW_MATRIX, modelViewMatrix_column);
    vislib::math::ShallowMatrix<GLfloat, 4, vislib::math::COLUMN_MAJOR> modelViewMatrix(&modelViewMatrix_column[0]);
    GLfloat projMatrix_column[16];
    glGetFloatv(GL_PROJECTION_MATRIX, projMatrix_column);
    vislib::math::ShallowMatrix<GLfloat, 4, vislib::math::COLUMN_MAJOR> projMatrix(&projMatrix_column[0]);
    vislib::math::Matrix<GLfloat, 4, vislib::math::COLUMN_MAJOR> modelViewMatrixInv(modelViewMatrix);
    modelViewMatrixInv.Invert();
    vislib::math::Matrix<GLfloat, 4, vislib::math::COLUMN_MAJOR> modelViewProjMatrix = projMatrix * modelViewMatrix;
    // end suck

    this->shader.Enable();

    // Transformation uniforms.
    glUniform4fv(this->shader.ParameterLocation("viewport"), 1, viewport);
    glUniformMatrix4fv(
        this->shader.ParameterLocation("modelViewProjection"), 1, GL_FALSE, modelViewProjMatrix.PeekComponents());
    glUniformMatrix4fv(
        this->shader.ParameterLocation("modelViewInverse"), 1, GL_FALSE, modelViewMatrixInv.PeekComponents());
    glUniformMatrix4fv(this->shader.ParameterLocation("modelView"), 1, GL_FALSE, modelViewMatrix.PeekComponents());

    // Other uniforms.
    glUniform1f(this->shader.ParameterLocation("kernelWidth"),
        this->kernelWidthParam.Param<core::param::FloatParam>()->Value());
    glUniform1f(this->shader.ParameterLocation("alphaScaling"),
        this->alphaScalingParam.Param<core::param::FloatParam>()->Value());
    glUniform1i(this->shader.ParameterLocation("attenuateSubpixel"),
        this->attenuateSubpixelParam.Param<core::param::BoolParam>()->Value() ? 1 : 0);

    // Color map uniforms.
    glEnable(GL_TEXTURE_1D);
    glActiveTexture(GL_TEXTURE0);
    unsigned int colTabSize = 0;
    auto tf = this->transferFunctionInSlot.CallAs<core::view::CallGetTransferFunction>();
    if (tf != nullptr && (*tf)()) {
        glBindTexture(GL_TEXTURE_1D, tf->OpenGLTexture());
        colTabSize = tf->TextureSize();
    }
    glUniform1i(this->shader.ParameterLocation("colorTable"), 0);
    glUniform3f(this->shader.ParameterLocation("colorConsts"), columnInfos[this->map.colorIdx].MinimumValue(),
        columnInfos[this->map.colorIdx].MaximumValue(), colTabSize);

    // Setup streaming.
    const GLuint numBuffers = 3;
    const GLuint bufferSize = 32 * 1024 * 1024;
    const float* data = this->floatTable->GetData();
    const GLuint dataStride = columnCount * sizeof(float);
    const GLuint dataItems = bufferSize / dataStride;
    const GLuint numChunks =
        this->rowSSBO.SetDataWithSize(data, dataStride, dataStride, dataItems, numBuffers, bufferSize);

    // For each chunk of rows, render all points in the lower half of the scatterplot matrix at once.
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, SSBOBindingPoint, rowSSBO.GetHandle());
    for (GLuint chunk = 0; chunk < numChunks; ++chunk) {
        GLuint numItems, sync;
        GLsizeiptr dstOffset, dstLength;
        rowSSBO.UploadChunk(chunk, numItems, sync, dstOffset, dstLength);
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, SSBOBindingPoint, this->rowSSBO.GetHandle(), dstOffset, dstLength);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        glDrawArraysInstanced(GL_POINTS, 0, static_cast<GLsizei>(numItems), plots.size());
        rowSSBO.SignalCompletion(sync);
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    this->shader.Disable();

    glDisable(GL_TEXTURE_1D);
    glDisable(GL_VERTEX_PROGRAM_POINT_SIZE);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void ScatterplotMatrixRenderer2D::drawLines(void) {
    /*
    auto aspect = this->scaleXParam.Param<core::param::FloatParam>()->Value();
    auto yScaling = this->scaleYParam.Param<core::param::FloatParam>()->Value();

    this->drawXAxis();
    this->drawYAxis();

    NVGcontext *ctx = static_cast<NVGcontext *>(this->nvgCtx);
    nvgSave(ctx);
    nvgTransform(ctx, this->nvgTrans.GetAt(0, 0), this->nvgTrans.GetAt(1, 0),
        this->nvgTrans.GetAt(0, 1), this->nvgTrans.GetAt(1, 1),
        this->nvgTrans.GetAt(0, 2), this->nvgTrans.GetAt(1, 2));

    nvgScale(ctx, this->scaleXParam.Param<core::param::FloatParam>()->Value(), 1.0f);

    float width = std::get<0>(this->viewport)*aspect;
    float height = std::get<1>(this->viewport)*yScaling;
    float midX = width / 2.0f;
    float midY = height / 2.0f;

    for (size_t s = 0; s < this->series.size(); s++) {
        if (this->series[s].size() < 8 || !this->selectedSeries[s]) {
            continue;
        }

        auto color = std::get<4>(this->columnSelectors[s]);

        nvgStrokeWidth(ctx, this->axisWidthParam.Param<core::param::FloatParam>()->Value());
        nvgStrokeColor(ctx, nvgRGBf(color[0], color[1], color[2]));
        nvgBeginPath(ctx);

        {
            float valX = this->series[s][0] - 0.5f;
            valX *= width;
            valX += midX;
            float valY = this->series[s][1] - 0.5f;
            valY *= height;
            valY += midY;
            nvgMoveTo(ctx, valX, valY);
        }

        for (size_t i = 1; i < this->series[s].size() / 4; i++) {
            float valX = this->series[s][i * 4] - 0.5f;
            valX *= width;
            valX += midX;
            float valY = this->series[s][i * 4 + 1] - 0.5f;
            valY *= height;
            valY += midY;
            nvgLineTo(ctx, valX, valY);
        }

        nvgStroke(ctx);
    }

    nvgRestore(ctx);
    */
}

void ScatterplotMatrixRenderer2D::drawXAxis(void) {
    /*
    auto numXTicks = this->axisTicksXParam.Param<core::param::IntParam>()->Value();
    auto aspect = this->scaleXParam.Param<core::param::FloatParam>()->Value();

    float minX = this->columnInfos[this->columnIdxs.abcissaIdx].MinimumValue();
    float maxX = this->columnInfos[this->columnIdxs.abcissaIdx].MaximumValue();

    std::vector<std::string> xTickText(numXTicks);
    float xTickLabel = (maxX - minX) / (numXTicks - 1);
    for (int i = 0; i < numXTicks; i++) {
        xTickText[i] = std::to_string(xTickLabel*i + minX);
    }

    float arrWidth = 0.05f;
    float arrHeight = 0.025f;
    float tickSize = this->nvgRenderInfo.fontSize;

    float dw = std::get<0>(this->viewport)*aspect;
    float dh = std::get<1>(this->viewport);
    float mw = dw / 2.0f;
    float mh = dh / 2.0f;

    NVGcontext *ctx = static_cast<NVGcontext *>(this->nvgCtx);
    nvgSave(ctx);
    nvgTransform(ctx, this->nvgTrans.GetAt(0, 0), this->nvgTrans.GetAt(1, 0),
        this->nvgTrans.GetAt(0, 1), this->nvgTrans.GetAt(1, 1),
        this->nvgTrans.GetAt(0, 2), this->nvgTrans.GetAt(1, 2));

    unsigned char col[4] = {255};
    core::utility::ColourParser::FromString(this->axisColorParam.Param<core::param::StringParam>()->Value(), 4, col);

    nvgStrokeWidth(ctx, 2.0f);
    nvgStrokeColor(ctx, nvgRGB(col[0], col[1], col[2]));

    nvgFontSize(ctx, this->nvgRenderInfo.fontSize*dh);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(ctx, nvgRGB(col[0], col[1], col[2]));

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, mw - dw / 2, mh - dh / 2);
    nvgLineTo(ctx, mw + (0.5f + arrWidth)*dw, mh - dh / 2);
    nvgMoveTo(ctx, mw + dw / 2, mh - (0.5f - arrHeight)*dh);
    nvgLineTo(ctx, mw + (0.5f + arrWidth)*dw, mh - dh / 2);
    nvgMoveTo(ctx, mw + dw / 2, mh - (0.5f + arrHeight)*dh);
    nvgLineTo(ctx, mw + (0.5f + arrWidth)*dw, mh - dh / 2);
    nvgStroke(ctx);

    float xTickOff = 1.0f / (numXTicks - 1);
    for (int i = 0; i < numXTicks; i++) {
        nvgBeginPath(ctx);
        nvgMoveTo(ctx, mw + (xTickOff*i - 0.5f)*dw, mh - dh / 2);
        nvgLineTo(ctx, mw + (xTickOff*i - 0.5f)*dw, mh - (0.5f + tickSize)*dh);
        nvgStroke(ctx);
    }

    nvgScale(ctx, 1.0f, -1.0f);
    nvgTranslate(ctx, 0.0f, -std::get<1>(this->viewport));

    for (int i = 0; i < numXTicks; i++) {
        nvgText(ctx, mw + (xTickOff*i - 0.5f)*dw, mh + (0.5f + tickSize)*dh, xTickText[i].c_str(), nullptr);
    }

    nvgRestore(ctx);
    */
}

void ScatterplotMatrixRenderer2D::drawYAxis(void) {
    /*
    TraceInfoCall *tic = this->getPointInfoSlot.CallAs<TraceInfoCall>();
    if (tic == nullptr) return;

    tic->SetRequest(TraceInfoCall::RequestType::GetClusterRanges, 0);
    if (!(*tic)(0)) return;

    auto clusterAddressRanges = tic->GetAddressRanges();
    auto clusterRanges = tic->GetRanges();


    int numYTicks = this->axisTicksYParam.Param<core::param::IntParam>()->Value();
    float aspect = this->scaleXParam.Param<core::param::FloatParam>()->Value();
    float yScaling = this->scaleYParam.Param<core::param::FloatParam>()->Value();

    std::vector<std::string> yTickText(numYTicks);

    float minY = std::get<0>(this->yRange);
    float maxY = std::get<1>(this->yRange);

    std::vector<float> yTicks(numYTicks);
    float yTickLabel = (maxY - minY) / (numYTicks - 1);
    float yTickOff = 1.0f / (numYTicks - 1);
    for (int i = 0; i < numYTicks; i++) {
        yTickText[i] = std::to_string(yTickLabel *i + minY);
        yTicks[i] = (1.0f / (numYTicks - 1)) * i;
    }

    float arrWidth = 0.025f;
    float arrHeight = 0.05f;
    float tickSize = this->nvgRenderInfo.fontSize*0.25f;

    float dw = std::get<0>(this->viewport); //< Should not care about aspect
    float dh = std::get<1>(this->viewport)*yScaling;
    float mw = dw / 2.0f;
    float mh = dh / 2.0f;

    NVGcontext *ctx = static_cast<NVGcontext *>(this->nvgCtx);
    nvgSave(ctx);
    nvgTransform(ctx, this->nvgTrans.GetAt(0, 0), this->nvgTrans.GetAt(1, 0),
        this->nvgTrans.GetAt(0, 1), this->nvgTrans.GetAt(1, 1),
        this->nvgTrans.GetAt(0, 2), this->nvgTrans.GetAt(1, 2));

    unsigned char col[4] = {255};
    core::utility::ColourParser::FromString(this->axisColorParam.Param<core::param::StringParam>()->Value(), 4, col);

    nvgStrokeWidth(ctx, 2.0f);
    nvgStrokeColor(ctx, nvgRGB(col[0], col[1], col[2]));

    nvgFontSize(ctx, this->nvgRenderInfo.fontSize*dh*0.5f);
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgFillColor(ctx, nvgRGB(col[0], col[1], col[2]));

    int offset = 20;

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, mw - dw / 2, mh - dh / 2);
    nvgLineTo(ctx, mw - dw / 2, offset + mh + (0.5f + arrHeight)*dh);
    nvgMoveTo(ctx, mw - (0.5f + arrWidth)*dw, offset + mh + dh / 2);
    nvgLineTo(ctx, mw - dw / 2, offset + mh + (0.5f + arrHeight)*dh);
    nvgMoveTo(ctx, mw - (0.5f - arrWidth)*dw, offset + mh + dh / 2);
    nvgLineTo(ctx, mw - dw / 2, offset + mh + (0.5f + arrHeight)*dh);
    nvgStroke(ctx);

    if (clusterRanges->size() < 2) return;



    // nullte
    float normY = this->columnInfos[std::get<1>(this->columnSelectors[0])].MaximumValue() -
    this->columnInfos[std::get<1>(this->columnSelectors[0])].MinimumValue();

    //float lastTick = mh - (0.5f - std::get<0>((*clusterRanges)[0]))*dh;
    float lastTick = (std::get<1>((*clusterRanges)[clusterRanges->size()-1]) / normY)*dh;
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, mw - dw / 2, lastTick);
    nvgLineTo(ctx, mw - (0.5f + tickSize)*dw, lastTick);
    nvgStroke(ctx);

    for (size_t i = clusterRanges->size() - 2; i >= 0; i--) {
        //float currentTick = mh - (0.5f - std::get<0>((*clusterRanges)[i]))*dh;
        float currentTick = (std::get<1>((*clusterRanges)[i]) / normY)*dh;

        if (std::abs(currentTick - lastTick) > 3.0f*this->nvgRenderInfo.fontSize*dh) {
            lastTick = currentTick;
            nvgBeginPath(ctx);
            nvgMoveTo(ctx, mw - dw / 2, lastTick);
            nvgLineTo(ctx, mw - (0.5f + tickSize)*dw, lastTick);
            nvgStroke(ctx);
        }

        if (i == 0) break;
    }

    nvgScale(ctx, 1.0f, -1.0f);
    nvgTranslate(ctx, 0.0f, -std::get<1>(this->viewport));

    // nullte
    //lastTick = mh - (0.5f - std::get<0>((*clusterRanges)[0]))*dh;
    lastTick = (std::get<1>((*clusterRanges)[clusterRanges->size() - 1]) / normY)*dh;

    std::stringstream ss;
    //ss << "0x" << std::setfill('0') << std::setw(16) << std::hex <<
    (*clusterAddressRanges)[clusterAddressRanges->size() - 1].first; ss << std::hex <<
    (*clusterAddressRanges)[clusterAddressRanges->size() - 1].first; nvgText(ctx, mw - (0.5f + tickSize)*dw,
    dh-lastTick, ss.str().c_str(), nullptr);

    for (size_t i = clusterRanges->size() - 2; i >= 0; i--) {
        //float currentTick = mh - (0.5f - std::get<0>((*clusterRanges)[i]))*dh;
        float currentTick = (std::get<1>((*clusterRanges)[i]) / normY)*dh;

        if (std::abs(currentTick - lastTick) > 3.0f*this->nvgRenderInfo.fontSize*dh) {
            lastTick = currentTick;
            std::stringstream ss;
            //ss << "0x" << std::setfill('0') << std::setw(16) << std::hex << (*clusterAddressRanges)[i].first;
            ss << std::hex << (*clusterAddressRanges)[i].first;
            nvgText(ctx, mw - (0.5f + tickSize)*dw, dh - lastTick, ss.str().c_str(), nullptr);
        }

        if (i == 0) break;
    }

    nvgRestore(ctx);
    */
}

void ScatterplotMatrixRenderer2D::drawToolTip(const float x, const float y, const std::string& text) const {
    /*auto ctx = static_cast<NVGcontext *>(this->nvgCtx);
    float ttOH = 10;
    float ttOW = 10;
    float ttW = 200;
    float ttH = ttOH + 6 * (ttOH + BND_WIDGET_HEIGHT);

    float heightOffset = ttOH + BND_WIDGET_HEIGHT;


    nvgBeginPath(ctx);
    nvgFontSize(ctx, 10.0f);
    nvgFillColor(ctx, nvgRGB(128, 128, 128));

    nvgRect(ctx, x, y, ttW, ttH);

    //bndTooltipBackground(ctx, x, y, ttW, ttH);
    bndTextField(ctx, x + ttOW, y + ttOH, ttW - 2 * ttOW, ttH, BND_CORNER_ALL, BND_DEFAULT, -1, text.c_str(), 0,
    text.size() - 1);

    nvgFill(ctx);
    */
}

size_t ScatterplotMatrixRenderer2D::searchAndDispPointAttr(const float x, const float y) {
    /*
    if (y >= 0.0f) { //< within scatterplot
        auto trans = this->oglTrans;
        trans.Invert();
        auto query_p = trans*vislib::math::Vector<float, 4>(x, y, 0.0f, 1.0f);
        float qp[2] = {query_p.X(), query_p.Y()};
        // search with nanoflann tree
        size_t idx[1] = {0};
        float dis[1] = {0.0f};
        this->tree->index->knnSearch(qp, 1, idx, dis);

        idx[0] = *reinterpret_cast<unsigned int *>(&this->series[0][idx[0] * 4 + 3]); //< toxic, which is the correct
    series?

        auto ssp = this->nvgTrans*vislib::math::Vector<float, 3>(x, y, 1.0f);
        TraceInfoCall *tic = this->getPointInfoSlot.CallAs<TraceInfoCall>();
        if (tic == nullptr) {
            // show tool tip
            this->drawToolTip(ssp.X() + 10, ssp.Y() + 10, std::string("No Info Call"));
        } else {
            tic->SetRequest(TraceInfoCall::RequestType::GetSymbolString, idx[0]);
            if (!(*tic)(0)) {
                // show tool tip
                this->drawToolTip(ssp.X() + 10, ssp.Y() + 10, std::string("No Info Found"));
            } else {
                auto st = tic->GetInfo();
                this->drawToolTip(ssp.X() + 10, ssp.Y() + 10, st);
            }
        }

        return idx[0];
    } else { //< within callstack
        // calculate depth
        // search for fitting range in chosen depth
        float boxHeight = std::get<1>(this->viewport) / 40.0f;
        float yCoord = std::fabsf(y);
        unsigned int depth = std::floorf(yCoord / boxHeight);
        auto ssp = this->nvgTrans*vislib::math::Vector<float, 3>(x, y, 1.0f);
        float aspect = this->scaleXParam.Param<core::param::FloatParam>()->Value();
        for (auto &r : this->callStack[depth]) {
            // rb / norm*dw
            float rb = std::get<0>(r);
            float re = std::get<1>(r);
            if ((rb / this->abcissa.size()*std::get<0>(this->viewport)*aspect) <= x && x <= (re /
    this->abcissa.size()*std::get<0>(this->viewport)*aspect)) { //< abcissa missing size_t symbolIdx = std::get<2>(r);
                TraceInfoCall *tic = this->getPointInfoSlot.CallAs<TraceInfoCall>();
                if (tic == nullptr) {
                    // show tool tip
                    this->drawToolTip(ssp.X() + 10, ssp.Y() + 10, std::string("No Info Call"));
                } else {
                    tic->SetRequest(TraceInfoCall::RequestType::GetSymbolString, symbolIdx);
                    if (!(*tic)(0)) {
                        // show tool tip
                        this->drawToolTip(ssp.X() + 10, ssp.Y() + 10, std::string("No Info Found"));
                    } else {
                        auto st = tic->GetInfo();
                        st += std::string(" ") + std::to_string((unsigned int)rb) + std::string(" ") +
    std::to_string((unsigned int)re); this->drawToolTip(ssp.X() + 10, ssp.Y() + 10, st);
                    }
                }
                return symbolIdx;
            }
        }
    }
    */
    return 0;
}
