/*
 OFX Transform plugin.
 
 Copyright (C) 2014 INRIA
 
 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:
 
 Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 
 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.
 
 Neither the name of the {organization} nor the names of its
 contributors may be used to endorse or promote products derived from
 this software without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
 INRIA
 Domaine de Voluceau
 Rocquencourt - B.P. 105
 78153 Le Chesnay Cedex - France
 
 
 The skeleton for this source file is from:
 OFX Basic Example plugin, a plugin that illustrates the use of the OFX Support library.
 
 Copyright (C) 2004-2005 The Open Effects Association Ltd
 Author Bruno Nicoletti bruno@thefoundry.co.uk
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 
 * Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.
 * Neither the name The Open Effects Association Ltd, nor the names of its
 contributors may be used to endorse or promote products derived from this
 software without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
 The Open Effects Association Ltd
 1 Wardour St
 London W1D 6PA
 England
 
 */

#include "Transform.h"

#include <cmath>
#ifdef _WINDOWS
#include <windows.h>
#endif

#include "../include/ofxsProcessing.H"
#include "GenericTransform.h"

#define kTranslateParamName "Translate"
#define kRotateParamName "Rotate"
#define kScaleParamName "Scale"
#define kSkewParamName "Skew"
#define kCenterParamName "Center"
#define kFilterParamName "Filter"

static const double pi=3.14159265358979323846264338327950288419717;


using namespace OFX;

class TransformProcessorBase : public OFX::ImageProcessor
{
    
public:
    
protected :
    
    OFX::Image *_srcImg;
    Transform2D::Matrix3x3 _transform;
    int _filter;
    OfxRectI _srcRod;
    
public :
    
    TransformProcessorBase(OFX::ImageEffect &instance)
    : OFX::ImageProcessor(instance)
    , _srcImg(0)
    {
        
    }
    
    void setSrcImg(OFX::Image *v) {_srcImg = v;}
    
    void setValues(const OfxPointD& translate,double rotate,const OfxPointD& scale,double skew,const OfxPointD& center,int filter,
                   const OfxRectI& srcRod)
    {
        
        _transform = Transform2D::Matrix3x3::getTransform(translate, scale, skew, rotate, center);
        _filter = filter;
        _srcRod = srcRod;
    }
    
    
};

static void normalize(double *x,double *y,double x1,double x2,double y1,double y2)
{
    *x = (*x - x1) / (x2 - x1);
    *y = (*y - y1) / (y2 - y1);
}

static void denormalize(double* x,double *y,const OfxRectD& rod)
{
    *x = *x * (rod.x2 - rod.x1);
    *y = *y * (rod.y2 - rod.y1);
}

template <class PIX, int nComponents, int maxValue>
class TransformProcessor : public TransformProcessorBase
{
    

public :
    TransformProcessor(OFX::ImageEffect &instance)
    : TransformProcessorBase(instance)
    {
        
    }
    
    void multiThreadProcessImages(OfxRectI procWindow)
    {
        
        for(int y = procWindow.y1; y < procWindow.y2; y++)
        {
            ///convert to a normalized 0 ,1 coordinate
            Transform2D::Point3D normalizedCoords;
            normalizedCoords.z = 1;
            normalizedCoords.y = (double)y; //(y - dstRod.y1) / (double)(dstRod.y2 - dstRod.y1);
            
            
            PIX *dstPix = (PIX *) _dstImg->getPixelAddress(procWindow.x1, y);
            for(int x = procWindow.x1; x < procWindow.x2; x++)
            {
                
                normalizedCoords.x = (double)x;//(x - dstRod.x1) / (double)(dstRod.x2 - dstRod.x1);
                Transform2D::Point3D transformed = _transform * normalizedCoords;
                
                //transformed.x = transformed.x * (double)(_srcRod.x2 - _srcRod.x1);
                // transformed.y = transformed.y * (double)(_srcRod.y2 - _srcRod.y1);
                if (transformed.z == 0.) {
                    // the back-transformed point is at infinity
                    for(int c = 0; c < nComponents; ++c) {
                        dstPix[c] = 0;
                    }
                } else {
                    double fx = transformed.x / transformed.z;
                    double fy = transformed.y / transformed.z;

                    if (_filter == 0) {
                        ///nearest neighboor
                        const int x = std::floor(fx+0.5);
                        const int y = std::floor(fy+0.5);

                        PIX *srcPix = (PIX *)  (_srcImg ? _srcImg->getPixelAddress(x, y) : 0);
                        if (srcPix) {
                            for(int c = 0; c < nComponents; ++c) {
                                dstPix[c] = srcPix[c];
                            }
                        } else {
                            for(int c = 0; c < nComponents; ++c) {
                                dstPix[c] = 0;
                            }
                        }
                    } else if (_filter == 1) {
                        // bilinear
                        const int x = std::floor(fx);
                        const int y = std::floor(fy);
                        const int nx = x + 1;
                        const int ny = y + 1;
                        const double dx = fx - x;
                        const double dy = fy - y;

                        PIX *cc = (PIX *)  (_srcImg ? _srcImg->getPixelAddress(x, y) : 0);
                        PIX *nc = (PIX *)  (_srcImg ? _srcImg->getPixelAddress(nx, y) : 0);
                        PIX *cn = (PIX *)  (_srcImg ? _srcImg->getPixelAddress(x, ny) : 0);
                        PIX *nn = (PIX *)  (_srcImg ? _srcImg->getPixelAddress(nx, ny) : 0);
                        if (cc && nc && cn && nn) {
                            for(int c = 0; c < nComponents; ++c) {
                                dstPix[c] = cc[c] + dx*(nc[c]-cc[c] + dy*(cc[c]+nn[c]-cn[c]-nc[c])) + dy*(cn[c]-cc[c]);
                            }
                        } else {
                            for(int c = 0; c < nComponents; ++c) {
                                dstPix[c] = 0;
                            }
                        }

                    }
                }
                dstPix += nComponents;
                
                
            }
        }
    }
};


////////////////////////////////////////////////////////////////////////////////
/** @brief The plugin that does our work */
class TransformPlugin : public OFX::ImageEffect {
    
   
    
protected :
    // do not need to delete these, the ImageEffect is managing them for us
    OFX::Clip *dstClip_;
    OFX::Clip *srcClip_;
    OFX::Double2DParam* _translate;
    OFX::DoubleParam* _rotate;
    OFX::Double2DParam* _scale;
    OFX::DoubleParam* _skew;
    OFX::Double2DParam* _center;
    OFX::ChoiceParam* _filter;
public :
    
    
    /** @brief ctor */
    TransformPlugin(OfxImageEffectHandle handle)
    : ImageEffect(handle)
    , dstClip_(0)
    , srcClip_(0)
    
    {
        dstClip_ = fetchClip(kOfxImageEffectOutputClipName);
        srcClip_ = fetchClip(kOfxImageEffectSimpleSourceClipName);
        _translate = fetchDouble2DParam(kTranslateParamName);
        _rotate = fetchDoubleParam(kRotateParamName);
        _scale = fetchDouble2DParam(kScaleParamName);
        _skew = fetchDoubleParam(kSkewParamName);
        _center = fetchDouble2DParam(kCenterParamName);
        _filter = fetchChoiceParam(kFilterParamName);
    }
    
    virtual bool getRegionOfDefinition(const RegionOfDefinitionArguments &args, OfxRectD &rod);
    
    /* Override the render */
    virtual void render(const OFX::RenderArguments &args);
    
    virtual bool isIdentity(const RenderArguments &args, Clip * &identityClip, double &identityTime);
    
    /* set up and run a processor */
    void setupAndProcess(TransformProcessorBase &, const OFX::RenderArguments &args);
};


////////////////////////////////////////////////////////////////////////////////
/** @brief render for the filter */

////////////////////////////////////////////////////////////////////////////////
// basic plugin render function, just a skelington to instantiate templates from

/* set up and run a processor */
void
TransformPlugin::setupAndProcess(TransformProcessorBase &processor, const OFX::RenderArguments &args)
{
    std::auto_ptr<OFX::Image> dst(dstClip_->fetchImage(args.time));
    std::auto_ptr<OFX::Image> src(srcClip_->fetchImage(args.time));
    if(src.get() && dst.get())
    {
        OFX::BitDepthEnum dstBitDepth       = dst->getPixelDepth();
        OFX::PixelComponentEnum dstComponents  = dst->getPixelComponents();
        OFX::BitDepthEnum    srcBitDepth      = src->getPixelDepth();
        OFX::PixelComponentEnum srcComponents = src->getPixelComponents();
        if(srcBitDepth != dstBitDepth || srcComponents != dstComponents)
            throw int(1);

        OfxPointD extent = getProjectExtent();
        OfxPointD offset = getProjectOffset();
        OfxPointD scale;
        OfxPointD translate;
        double rotate;
        OfxPointD center;
        int filter;
        double skew;
        _scale->getValue(scale.x, scale.y);
        _translate->getValue(translate.x, translate.y);
        translate.x *= (extent.x - offset.x);
        translate.y *= (extent.y - offset.y);
        _rotate->getValue(rotate);
        
        ///convert to radians
        rotate = rotate * pi / 180.0;
        
        _center->getValue(center.x, center.y);
        center.x *= (extent.x - offset.x);
        center.y *= (extent.y - offset.y);
        _filter->getValue(filter);
        _skew->getValue(skew);
        processor.setValues(translate, rotate, scale, skew, center, filter,src->getRegionOfDefinition());
    }

    
    processor.setDstImg(dst.get());
    processor.setSrcImg(src.get());
    processor.setRenderWindow(args.renderWindow);
    processor.process();
}

bool
TransformPlugin::getRegionOfDefinition(const RegionOfDefinitionArguments &args, OfxRectD &rod)
{

    OfxRectD srcRoD = srcClip_->getRegionOfDefinition(args.time);
    OfxPointD extent = getProjectExtent();
    OfxPointD offset = getProjectOffset();
    OfxPointD scale;
    OfxPointD translate;
    double rotate;
    OfxPointD center;
    int filter;
    double skew;
    _scale->getValue(scale.x, scale.y);
    _translate->getValue(translate.x, translate.y);
    translate.x *= (extent.x - offset.x);
    translate.y *= (extent.y - offset.y);
    _rotate->getValue(rotate);
    
    ///convert to radians
    rotate = rotate * pi / 180.0;
    _center->getValue(center.x, center.y);
    center.x *= (extent.x - offset.x);
    center.y *= (extent.y - offset.y);
    _filter->getValue(filter);
    _skew->getValue(skew);
    

    Transform2D::Matrix3x3 transform = Transform2D::Matrix3x3::getTransform(translate, scale, skew, rotate, center);
    transform = transform.invert();
    /// now transform the 4 corners of the initial image
    Transform2D::Point3D topLeft = transform * Transform2D::Point3D(srcRoD.x1,srcRoD.y2,1);
    Transform2D::Point3D topRight = transform * Transform2D::Point3D(srcRoD.x2,srcRoD.y2,1);
    Transform2D::Point3D bottomLeft = transform * Transform2D::Point3D(srcRoD.x1,srcRoD.y1,1);
    Transform2D::Point3D bottomRight = transform * Transform2D::Point3D(srcRoD.x2,srcRoD.y1,1);
    
    double l = std::min(std::min(std::min(topLeft.x, bottomLeft.x),topRight.x),bottomRight.x);
    double b = std::min(std::min(std::min(bottomLeft.y, bottomRight.y),topLeft.y),topRight.y);
    double r = std::max(std::max(std::max(topRight.x, bottomRight.x),topLeft.x),bottomLeft.x);
    double t = std::max(std::max(std::max(topLeft.y, topRight.y),bottomLeft.y),bottomRight.y);
    
    //  denormalize(&l, &b, srcRoD);
    // denormalize(&r, &t, srcRoD);

    rod.x1 = (int)std::ceil(l);
    rod.x2 = (int)std::floor(r);
    rod.y1 = (int)std::ceil(b);
    rod.y2 = (int)std::floor(t);
    return true;
}

// the overridden render function
void
TransformPlugin::render(const OFX::RenderArguments &args)
{
    
    // instantiate the render code based on the pixel depth of the dst clip
    OFX::BitDepthEnum       dstBitDepth    = dstClip_->getPixelDepth();
    OFX::PixelComponentEnum dstComponents  = dstClip_->getPixelComponents();
    
    assert(dstComponents == OFX::ePixelComponentRGB || dstComponents == OFX::ePixelComponentRGBA);
    if(dstComponents == OFX::ePixelComponentRGBA)
    {
        switch(dstBitDepth)
        {
            case OFX::eBitDepthUByte :
            {
                TransformProcessor<unsigned char, 4, 255> fred(*this);
                setupAndProcess(fred, args);
                break;
            }
            case OFX::eBitDepthUShort :
            {
                TransformProcessor<unsigned short, 4, 65535> fred(*this);
                setupAndProcess(fred, args);
                break;
            }
            case OFX::eBitDepthFloat :
            {
                TransformProcessor<float,4,1> fred(*this);
                setupAndProcess(fred, args);
                break;
            }
            default :
                OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }
    }
    else
    {
        switch(dstBitDepth)
        {
            case OFX::eBitDepthUByte :
            {
                TransformProcessor<unsigned char, 3, 255> fred(*this);
                setupAndProcess(fred, args);
                break;
            }
            case OFX::eBitDepthUShort :
            {
                TransformProcessor<unsigned short, 3, 65535> fred(*this);
                setupAndProcess(fred, args);
                break;
            }
            case OFX::eBitDepthFloat :
            {
                TransformProcessor<float,3,1> fred(*this);
                setupAndProcess(fred, args);
                break;
            }
            default :
                OFX::throwSuiteStatusException(kOfxStatErrUnsupported);
        }
    }
}

bool TransformPlugin::isIdentity(const RenderArguments &args, Clip * &identityClip, double &identityTime)
{
    OfxPointD scale;
    OfxPointD translate;
    double rotate;
    double skew;
    _scale->getValue(scale.x, scale.y);
    _translate->getValue(translate.x, translate.y);
    _rotate->getValue(rotate);
    _skew->getValue(skew);

    if (scale.x == 1. && scale.y == 1. && translate.x == 0 && translate.y == 0 && rotate == 0 && skew == 0) {
        identityClip = srcClip_;
        identityTime = args.time;
        return true;
    }
    return false;
}

void TransformPluginFactory::load()
{
    
}

void TransformPluginFactory::describe(OFX::ImageEffectDescriptor &desc)
{
    // basic labels
    desc.setLabels("TransformOFX", "TransformOFX", "TransformOFX");
    desc.setPluginGrouping("Transform");
    desc.setPluginDescription("Translate / Rotate / Scale a 2D image.");
    
    desc.addSupportedContext(eContextFilter);
    desc.addSupportedContext(eContextGeneral);
    desc.addSupportedContext(eContextPaint);
    desc.addSupportedBitDepth(eBitDepthUByte);
    desc.addSupportedBitDepth(eBitDepthUShort);
    desc.addSupportedBitDepth(eBitDepthFloat);
    
    // set a few flags
    desc.setSingleInstance(false);
    desc.setHostFrameThreading(false);
    desc.setSupportsMultiResolution(true);
    

    desc.setSupportsTiles(false);
    desc.setTemporalClipAccess(false);
    desc.setRenderTwiceAlways(false);
    desc.setSupportsMultipleClipPARs(false);
    desc.setRenderThreadSafety(OFX::eRenderFullySafe);
}


void TransformPluginFactory::describeInContext(OFX::ImageEffectDescriptor &desc, OFX::ContextEnum context)
{
    if (!OFX::getImageEffectHostDescription()->supportsParametricParameter) {
        throwHostMissingSuiteException(kOfxParametricParameterSuite);
    }

    // Source clip only in the filter context
    // create the mandated source clip
    ClipDescriptor *srcClip = desc.defineClip(kOfxImageEffectSimpleSourceClipName);
    srcClip->addSupportedComponent(ePixelComponentRGBA);
    srcClip->addSupportedComponent(ePixelComponentRGB);
    srcClip->setTemporalClipAccess(false);
    srcClip->setSupportsTiles(true);
    srcClip->setIsMask(false);
    
    // create the mandated output clip
    ClipDescriptor *dstClip = desc.defineClip(kOfxImageEffectOutputClipName);
    dstClip->addSupportedComponent(ePixelComponentRGBA);
    dstClip->setSupportsTiles(true);
    
    
    // make some pages and to things in
    PageParamDescriptor *page = desc.definePageParam("Controls");
    Double2DParamDescriptor* translate = desc.defineDouble2DParam(kTranslateParamName);
    translate->setLabels(kTranslateParamName, kTranslateParamName, kTranslateParamName);
    translate->setDoubleType(OFX::eDoubleTypeNormalisedXY);
    translate->setDefault(0, 0);
    page->addChild(*translate);
    
    DoubleParamDescriptor* rotate = desc.defineDoubleParam(kRotateParamName);
    rotate->setLabels(kRotateParamName, kRotateParamName, kRotateParamName);
    rotate->setDefault(0);
    rotate->setRange(-180, 180);
    rotate->setDisplayRange(-180, 180);
    page->addChild(*rotate);
    
    Double2DParamDescriptor* scale = desc.defineDouble2DParam(kScaleParamName);
    scale->setLabels(kScaleParamName, kScaleParamName, kScaleParamName);
    scale->setDefault(1,1);
    scale->setRange(0.1,0.1,10,10);
    scale->setDisplayRange(0.1, 0.1, 10, 10);
    page->addChild(*scale);
    
    DoubleParamDescriptor* skew = desc.defineDoubleParam(kSkewParamName);
    skew->setLabels(kSkewParamName, kSkewParamName, kSkewParamName);
    skew->setDefault(0);
    skew->setRange(-1, 1);
    skew->setDisplayRange(-1,1);
    page->addChild(*skew);
    
    Double2DParamDescriptor* center = desc.defineDouble2DParam(kCenterParamName);
    center->setLabels(kCenterParamName, kCenterParamName, kCenterParamName);
    center->setDoubleType(OFX::eDoubleTypeNormalisedXY);
    center->setDefault(0.5, 0.5);
    page->addChild(*center);
    
    ChoiceParamDescriptor* filter = desc.defineChoiceParam(kFilterParamName);
    filter->setLabels(kFilterParamName, kFilterParamName, kFilterParamName);
    filter->setDefault(0);
    filter->appendOption("Nearest neighboor");
    filter->appendOption("Bilinear");
    filter->appendOption("Bicubic");
    
}

OFX::ImageEffect* TransformPluginFactory::createInstance(OfxImageEffectHandle handle, OFX::ContextEnum context)
{
    return new TransformPlugin(handle);
}
