#ifndef COREOBJECTFACTORYBASE_H_INCLUDED
#define COREOBJECTFACTORYBASE_H_INCLUDED

#include "mitkMapper.h"
#include <itkObjectFactoryBase.h>
#include <itkVersion.h>

namespace mitk {

class DataTreeNode;

//## @brief base-class for factories of certain mitk objects. 
//## @ingroup Algorithms
//## This interface can be implemented by factories which add new mapper classes or extend the
//## data tree deserialization mechanism.

class CoreObjectFactoryBase : public itk::ObjectFactoryBase
{
  public:
    mitkClassMacro(CoreObjectFactoryBase,itk::ObjectFactoryBase);
    virtual Mapper::Pointer CreateMapper(mitk::DataTreeNode* node, MapperSlotId slotId) = 0;
    virtual itk::Object::Pointer CreateCoreObject( const std::string& className ) = 0;
    virtual const char* GetFileExtensions() = 0;
    virtual const char* GetSaveFileExtensions() = 0;
    virtual const char* GetITKSourceVersion() const
    { 
      return ITK_SOURCE_VERSION;
    }
    virtual const char* GetDescription() const
    {
      return "Core Object Factory";
    }
};

template <class T>
class CreateOverrideObjectFunction : public itk::CreateObjectFunctionBase
{
public:
  /** Standard class typedefs. */
  typedef CreateOverrideObjectFunction  Self;
  typedef itk::SmartPointer<Self>    Pointer;
    
  /** Methods from itk:LightObject. */
  itkFactorylessNewMacro(Self);
  LightObject::Pointer CreateObject() { typename T::Pointer p = T::New(); 
    p->Register();
    return p.GetPointer(); 
  }

protected:
  CreateOverrideObjectFunction() {}
  ~CreateOverrideObjectFunction() {}
  
private:
  CreateOverrideObjectFunction(const Self&); //purposely not implemented
  void operator=(const Self&); //purposely not implemented
};

}
#endif
