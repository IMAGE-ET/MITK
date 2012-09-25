/*===================================================================

The Medical Imaging Interaction Toolkit (MITK)

Copyright (c) German Cancer Research Center,
Division of Medical and Biological Informatics.
All rights reserved.

This software is distributed WITHOUT ANY WARRANTY; without
even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.

See LICENSE.txt or http://www.mitk.org for details.

===================================================================*/

#ifndef MITKIMAGEPIXELWRITEACCESSOR_H
#define MITKIMAGEPIXELWRITEACCESSOR_H

#include "mitkImagePixelAccessor.h"
#include "mitkImageWriteAccessor.h"

namespace mitk {

//##Documentation
//## @brief Gives locked and index-based write access for a particular image part.
//## The class provides several set- and get-methods, which allow an easy pixel access.
//## It needs to know about pixel type and dimension of its image at compile time.
//## @tparam TPixel defines the PixelType
//## @tparam VDimension defines the dimension for accessing data
//## @ingroup Data
template <class TPixel, unsigned int VDimension = 3>
class ImagePixelWriteAccessor : public ImagePixelAccessor<TPixel, VDimension>
{
  friend class Image;

public:
  /** \brief Instantiates a mitk::ImageWriteAccessor (see its doxygen page for more details)
     *  \param Image::Pointer specifies the associated Image
     *  \param ImageDataItem* specifies the allocated image part
     *  \param OptionFlags properties from mitk::ImageAccessorBase::Options can be chosen and assembled with bitwise unification.
     *  \throws mitk::Exception if the Constructor was created inappropriately
     *  \throws mitk::MemoryIsLockedException if requested image area is exclusively locked and mitk::ImageAccessorBase::ExceptionIfLocked is set in OptionFlags
     *
     *   Includes a check if typeid of PixelType coincides with templated TPixel
     *   and a check if VDimension equals to the Dimension of the Image.*/
  ImagePixelWriteAccessor(
      ImagePointer iP,
      ImageDataItem* iDI = NULL,
      int OptionFlags = ImageAccessorBase::DefaultBehavior
      ) :
    ImagePixelAccessor<TPixel, VDimension>(iP,iDI),
    m_WriteAccessor(iP , iDI, OptionFlags)
  {

    // Check if PixelType is correct
    if(m_WriteAccessor.m_Image->GetPixelType().GetTypeId() != typeid(TPixel))
    {
      mitkThrow() << "Invalid ImageAccessor: PixelTypes of Image and ImageAccessor are not equal";
    }

    // Check if Dimensions are correct
    if(ImagePixelAccessor<TPixel,VDimension>::m_ImageDataItem == NULL) {
      if(m_WriteAccessor.m_Image->GetDimension() != VDimension)
        mitkThrow() << "Invalid ImageAccessor: The Dimensions of ImageAccessor and Image are not equal. They have to be equal if an entire image is requested";
    }
    else {
      if(ImagePixelAccessor<TPixel,VDimension>::m_ImageDataItem->GetDimension() != VDimension)
        mitkThrow() << "Invalid ImageAccessor: The Dimensions of ImageAccessor and ImageDataItem are not equal.";
    }

  }


/** \brief Gives full data access. */
    virtual inline TPixel * GetData()
    {
      return (TPixel*) m_WriteAccessor.m_AddressBegin;
    }



  /// Sets a pixel value at given index.
  void SetPixelByIndex(const itk::Index<VDimension>& idx, const TPixel & value)
  {
    const unsigned int * imageDims = ImagePixelAccessor<TPixel,VDimension>::m_ImageDataItem->m_Dimensions;

    unsigned int offset = 0;

    switch(VDimension)
    {
      case 4:
        offset += idx[3]*imageDims[0]*imageDims[1]*imageDims[2];
      case 3:
        offset += idx[2]*imageDims[0]*imageDims[1];
      case 2:
        offset += idx[0] + idx[1]*imageDims[0];
    }

    *(((TPixel*)m_WriteAccessor.m_AddressBegin) + offset) = value;
  }


  /** Extends SetPixel by integrating index validation to prevent overflow. */
  void SetPixelByIndexSafe(const itk::Index<VDimension>& idx, const TPixel & value) {
    const unsigned int * imageDims = ImagePixelAccessor<TPixel,VDimension>::m_ImageDataItem->m_Dimensions;

    unsigned int offset = 0;

    switch(VDimension)
    {
      case 4:
        offset += idx[3]*imageDims[0]*imageDims[1]*imageDims[2];
      case 3:
        offset += idx[2]*imageDims[0]*imageDims[1];
      case 2:
        offset += idx[0] + idx[1]*imageDims[0];
    }

    TPixel* targetAddress = ((TPixel*)m_WriteAccessor.m_AddressBegin) + offset;

    if(targetAddress >= m_WriteAccessor.m_AddressBegin && targetAddress < m_WriteAccessor.m_AddressEnd)
    {
      *targetAddress = value;
    }
    else
    {
      printf("image dimensions = %d, %d, %d\n", imageDims[0], imageDims[1], imageDims[2]);
      printf("m_AddressBegin: %p, m_AddressEnd: %p, offset: %u\n", m_WriteAccessor.m_AddressBegin, m_WriteAccessor.m_AddressEnd, offset);
      mitkThrow() << "ImageAccessor Overflow: image access exceeds the requested image area at " << idx << ".";
    }

  }

  /** Returns a reference to the pixel at given world coordinate */
  void SetPixelByWorldCoordinates(const mitk::Point3D&, const TPixel & value, unsigned int timestep = 0);

  virtual ~ImagePixelWriteAccessor()
  {

  }

private:

  ImageWriteAccessor m_WriteAccessor;

};

}
#endif // MITKIMAGEWRITEACCESSOR_H
