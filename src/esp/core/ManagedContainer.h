// Copyright (c) Facebook, Inc. and its affiliates.
// This source code is licensed under the MIT license found in the
// LICENSE file in the root directory of this source tree.

#ifndef ESP_CORE_MANAGEDCONTAINER_H_
#define ESP_CORE_MANAGEDCONTAINER_H_

/** @file
 * @brief Class Template @ref esp::core::ManagedContainer : container
 * functionality to manage @ref esp::core::AbstractManagedObject objects
 */

#include "ManagedContainerBase.h"

namespace esp {
namespace core {
/**
 * @brief Class template defining responsibilities and functionality for
 * managing @ref esp::core::AbstractManagedObject constructs.
 * @tparam ManagedPtr the type of managed object a particular specialization of
 * this class works with.  Must inherit from @ref
 * esp::core::AbstractManagedObject.
 */
template <class T>
class ManagedContainer : public ManagedContainerBase {
 public:
  static_assert(std::is_base_of<AbstractManagedObject, T>::value,
                "ManagedContainer :: Managed object type must be derived from "
                "AbstractManagedObject");

  typedef std::shared_ptr<T> ManagedPtr;

  ManagedContainer(const std::string& metadataType)
      : ManagedContainerBase(metadataType) {}

  /**
   * @brief Creates an instance of a managed object described by passed string.
   *
   * If a managed object exists with this handle, the existing managed object
   * will be overwritten with the newly created one if @ref
   * registerObject is true.
   *
   * @param objectHandle the origin of the desired managed object to be
   * created.
   * @param registerObject whether to add this managed object to the
   * library or not. If the user is going to edit this managed object, this
   * should be false. Defaults to true. If specified as true, then this function
   * returns a copy of the registered managed object.
   * @return a reference to the desired managed object.
   */
  virtual ManagedPtr createObject(const std::string& objectHandle,
                                  bool registerObject = true) = 0;

  /**
   * @brief Creates an instance of a managed object holding default values.
   *
   * If a managed object exists with this handle, the existing managed object
   * will be overwritten with the newly created one if @ref
   * registerObject is true. This method is specifically intended to
   * directly construct an managed object for editing, and so
   * defaults to false for @ref registerObject
   *
   * @param objectName The desired handle for this managed object.
   * @param registerObject whether to add this managed object to the
   * library or not. If the user is going to edit this managed object, this
   * should be false. If specified as true, then this function returns a copy of
   * the registered managed object. Defaults to false. If specified as true,
   * then this function returns a copy of the registered managed object.
   * @return a reference to the desired managed object.
   */
  ManagedPtr createDefaultObject(const std::string& objectName,
                                 bool registerObject = false) {
    // create default managed object
    ManagedPtr object = this->initNewObjectInternal(objectName, false);
    if (nullptr == object) {
      return nullptr;
    }
    return this->postCreateRegister(object, registerObject);
  }  // ManagedContainer::createDefault

  /**
   * @brief Creates an instance of a managed object from a JSON file.
   *
   * @param filename the name of the file describing the object managed object.
   * Assumes it exists and fails if it does not.
   * @param registerObject whether to add this managed object to the
   * library. If the user is going to edit this managed object, this should be
   * false - any subsequent editing will require re-registration. Defaults to
   * true.
   * @return a reference to the desired managed object, or nullptr if fails.
   */
  ManagedPtr createObjectFromJSONFile(const std::string& filename,
                                      bool registerObject = true) {
    io::JsonDocument docConfig;
    bool success = this->verifyLoadDocument(filename, docConfig);
    if (!success) {
      LOG(ERROR) << "ManagedContainer::createObjectFromFile ("
                 << this->objectType_
                 << ") : Failure reading document as JSON : " << filename
                 << ". Aborting.";
      return nullptr;
    }
    // convert doc to const value
    const io::JsonGenericValue config = docConfig.GetObject();
    ManagedPtr attr = this->buildManagedObjectFromDoc(filename, config);
    return this->postCreateRegister(attr, registerObject);
  }  // ManagedContainer::createObjectFromJSONFile

  /**
   * @brief Method to load a Managed Object's data from a file.  If the file
   * type is not supported by specialization of this method, this method
   * executes and an error is thrown.
   * @tparam type of document to load.
   * @param filename name of file document to load from
   * @param config document to read for data
   * @return a shared pointer of the created Managed Object
   */
  template <typename U>
  ManagedPtr buildManagedObjectFromDoc(const std::string& filename,
                                       CORRADE_UNUSED const U& config) {
    LOG(ERROR)
        << "ManagedContainer::buildManagedObjectFromDoc (" << this->objectType_
        << ") : Failure loading attributes from document of unknown type : "
        << filename << ". Aborting.";
  }
  /**
   * @brief Method to load a Managed Object's data from a file.  This is the
   * JSON specialization, using type inference.
   * @param filename name of file document to load from
   * @param config JSON document to read for data
   * @return a shared pointer of the created Managed Object
   */
  ManagedPtr buildManagedObjectFromDoc(const std::string& filename,
                                       const io::JsonGenericValue& jsonConfig) {
    return this->buildObjectFromJSONDoc(filename, jsonConfig);
  }

  /**
   * @brief Parse passed JSON Document specifically for @ref ManagedPtr object.
   * It always returns a @ref ManagedPtr object.
   * @param filename The name of the file describing the @ref ManagedPtr,
   * used as managed object handle/name on create.
   * @param jsonConfig json document to parse - assumed to be legal JSON doc.
   * @return a reference to the desired managed object.
   */
  virtual ManagedPtr buildObjectFromJSONDoc(
      const std::string& filename,
      const io::JsonGenericValue& jsonConfig) = 0;

  /**
   * @brief Add a copy of @ref esp::core::AbstractManagedObject to the @ref
   * objectLibrary_.
   *
   * @param managedObject The managed object.
   * @param objectHandle The key for referencing the managed object in
   * the @ref objectLibrary_. Will be set as origin handle for managed
   * object. If empty string, use existing origin handle.
   * @param forceRegistration Will register object even if conditional
   * registration checks fail in registerObjectFinalize.
   *
   * @return The unique ID of the managed object being registered, or
   * ID_UNDEFINED if failed
   */
  int registerObject(ManagedPtr managedObject,
                     const std::string& objectHandle = "",
                     bool forceRegistration = false) {
    if (nullptr == managedObject) {
      LOG(ERROR) << "ManagedContainer::registerObject : Invalid "
                    "(null) managed object passed to registration. Aborting.";
      return ID_UNDEFINED;
    }
    if ("" != objectHandle) {
      return registerObjectFinalize(managedObject, objectHandle,
                                    forceRegistration);
    }
    std::string handleToSet = managedObject->getHandle();
    if ("" == handleToSet) {
      LOG(ERROR) << "ManagedContainer::registerObject : No "
                    "valid handle specified for "
                 << objectType_ << " managed object to register. Aborting.";
      return ID_UNDEFINED;
    }
    return registerObjectFinalize(managedObject, handleToSet,
                                  forceRegistration);
  }  // ManagedContainer::registerObject

  /**
   * @brief Register managed object and call appropriate ResourceManager method
   * to execute appropriate post-registration processes due to changes in the
   * managed object. Use if user wishes to update existing objects built by
   * managed object with new managed object data and such objects support this
   * kind of update. Requires the use of managed object's assigned handle in
   * order to reference existing constructions built from the original version
   * of this managed object.
   * @param managedObject The managed object.
   * @return The unique ID of the managed object being registered, or
   * ID_UNDEFINED if failed
   */
  int registerObjectAndUpdate(ManagedPtr managedObject) {
    std::string originalHandle = managedObject->getHandle();
    int ID = this->registerObject(managedObject, originalHandle);
    // If undefined then some error occurred.
    if (ID_UNDEFINED == ID) {
      return ID_UNDEFINED;
    }
    // TODO : call Resource Manager for post-registration processing of this
    // managed object

    return ID;
  }  // ManagedContainer::registerObjectAndUpdate

  /**
   * @brief Get a reference to the managed object identified by the
   * managedObjectID.  Should only be used internally. Users should
   * only ever access copies of managed objects.
   *
   * Can be used to manipulate a managed object before instancing new objects.
   * @param managedObjectID The ID of the managed object. Is mapped to the key
   * referencing the asset in @ref objectLibrary_ by @ref
   * objectLibKeyByID_.
   * @return A mutable reference to the object managed object, or nullptr if
   * does not exist
   */
  ManagedPtr getObjectByID(int managedObjectID) const {
    std::string objectHandle = getObjectHandleByID(managedObjectID);
    if (!checkExistsWithMessage(objectHandle,
                                "ManagedContainer::getObjectByID")) {
      return nullptr;
    }
    return getObjectInternal<T>(objectHandle);
  }  // ManagedContainer::getObjectByID

  /**
   * @brief Get a reference to the managed object for the asset
   * identified by the passed objectHandle.  Should only be used
   * internally. Users should only ever access copies of managed objects.
   *
   * @param objectHandle The key referencing the asset in @ref
   * objectLibrary_.
   * @return A reference to the managed object, or nullptr if does not
   * exist
   */
  ManagedPtr getObjectByHandle(const std::string& objectHandle) const {
    if (!checkExistsWithMessage(objectHandle,
                                "ManagedContainer::getObjectByHandle")) {
      return nullptr;
    }

    return getObjectInternal<T>(objectHandle);
  }  // ManagedContainer::getObject

  /**
   * @brief Remove the managed object referenced by the passed string handle.
   * Will emplace managed object ID within deque of usable IDs and return the
   * managed object being removed.
   * @param objectID The ID of the managed object desired.
   * @return the desired managed object being deleted, or nullptr if does not
   * exist
   */
  ManagedPtr removeObjectByID(int objectID) {
    std::string objectHandle = getObjectHandleByID(objectID);
    if (!checkExistsWithMessage(objectHandle,
                                "ManagedContainer::removeObjectByID")) {
      return nullptr;
    }
    return removeObjectInternal(objectHandle,
                                "ManagedContainer::removeObjectByID");
  }

  /**
   * @brief  Remove the managed object referenced by the passed string handle.
   * Will emplace managed object ID within deque of usable IDs and return the
   * managed object being removed.
   * @param objectHandle the string key of the managed object desired.
   * @return the desired managed object being deleted, or nullptr if does not
   * exist
   */
  ManagedPtr removeObjectByHandle(const std::string& objectHandle) {
    return removeObjectInternal(objectHandle,
                                "ManagedContainer::removeObjectByHandle");
  }

  /**
   * @brief Remove all managed objects that have not been marked as
   * default/non-removable, and return a vector of the managed objects removed.
   * @return A vector containing all the managed objects that have been removed
   * from the library.
   */
  std::vector<ManagedPtr> removeAllObjects() {
    return removeObjectsBySubstring();
  }  // removeAllObjects

  /**
   * @brief remove managed objects that contain passed substring and that have
   * not been marked as default/non-removable, and return a vector of the
   * managed objects removed.
   * @param subStr substring to search for within existing primitive object
   * managed objects
   * @param contains whether to search for keys containing, or excluding,
   * substr
   * @return A vector containing all the managed objects that have been removed
   * from the library.
   */
  std::vector<ManagedPtr> removeObjectsBySubstring(
      const std::string& subStr = "",
      bool contains = true);

  /**
   * @brief Get the ID of the managed object in @ref objectLibrary_ for
   * the given managed object Handle, if exists.
   *
   * @param objectHandle The string key referencing the managed object in
   * @ref objectLibrary_. Usually the origin handle.
   * @return The object ID for the managed object with the passed handle, or
   * ID_UNDEFINED if none exists.
   */
  int getObjectIDByHandle(const std::string& objectHandle) {
    return getObjectIDByHandleOrNew(objectHandle, false);
  }  // ManagedContainer::getObjectIDByHandle

  /**
   * @brief Get a copy of the managed object identified by the
   * managedObjectID.
   *
   * Can be used to manipulate a managed object before instancing new objects.
   * @param managedObjectID The ID of the managed object. Is mapped to the key
   * referencing the asset in @ref objectLibrary_ by @ref
   * objectLibKeyByID_.
   * @return A mutable reference to the object managed object, or nullptr if
   * does not exist
   */
  ManagedPtr getObjectCopyByID(int managedObjectID) {
    std::string objectHandle = getObjectHandleByID(managedObjectID);
    if (!checkExistsWithMessage(objectHandle,
                                "ManagedContainer::getObjectCopyByID")) {
      return nullptr;
    }
    auto orig = getObjectInternal<T>(objectHandle);
    return this->copyObject(orig);
  }  // ManagedContainer::getObjectCopyByID

  /**
   * @brief Return a reference to a copy of the object specified
   * by passed handle. This is the version that should be accessed by the
   * user.
   * @param objectHandle the string key of the managed object desired.
   * @return a copy of the desired managed object, or nullptr if does
   * not exist
   */
  ManagedPtr getObjectCopyByHandle(const std::string& objectHandle) {
    if (!checkExistsWithMessage(objectHandle,
                                "ManagedContainer::getObjectCopyByHandle")) {
      return nullptr;
    }
    auto orig = getObjectInternal<T>(objectHandle);
    return this->copyObject(orig);
  }  // ManagedContainer::getObjectCopyByHandle

  /**
   * @brief Get a copy of the managed object identified by the
   * managedObjectID, casted to the appropriate derived managed object class.
   *
   * Can be used to manipulate a managed object before instancing new objects.
   * @param managedObjectID The ID of the managed object. Is mapped to the key
   * referencing the asset in @ref objectLibrary_ by @ref
   * objectLibKeyByID_.
   * @return A mutable reference to the object managed object, or nullptr if
   * does not exist
   */
  template <class U>
  std::shared_ptr<U> getObjectCopyByID(int managedObjectID) {
    // call non-template version
    std::string objectHandle = getObjectHandleByID(managedObjectID);
    auto res = getObjectCopyByID(managedObjectID);
    if (nullptr == res) {
      return nullptr;
    }
    return std::dynamic_pointer_cast<U>(res);
  }  // ManagedContainer::getObjectCopyByID

  /**
   * @brief Return a reference to a copy of the object specified
   * by passed handle, casted to the appropriate derived managed object class.
   * This is the version that should be accessed by the user
   * @param objectHandle the string key of the managed object desired.
   * @return a copy of the desired managed object, or nullptr if does
   * not exist
   */
  template <class U>
  std::shared_ptr<U> getObjectCopyByHandle(const std::string& objectHandle) {
    // call non-template version
    auto res = getObjectCopyByHandle(objectHandle);
    if (nullptr == res) {
      return nullptr;
    }
    return std::dynamic_pointer_cast<U>(res);
  }  // ManagedContainer::getObjectCopyByHandle

  /**
   * @brief Set the object to provide default values upon construction of @ref
   * esp::core::AbstractManagedObject.  Override if object should not have
   * defaults
   * @param _defaultObj the object to use for defaults;
   */
  virtual void setDefaultObject(ManagedPtr& _defaultObj) {
    defaultObj_ = _defaultObj;
  }

  /**
   * @brief Clear any default objects used for construction.
   */
  void clearDefaultObject() { defaultObj_ = nullptr; }

 protected:
  //======== Internally accessed functions ========
  /**
   * @brief Perform post creation registration if specified.
   *
   * @param object The managed object
   * @param doRegistration If managed object should be registered
   * @return managed object, or null ptr if registration failed.
   */
  inline ManagedPtr postCreateRegister(ManagedPtr object, bool doRegistration) {
    if (!doRegistration) {
      return object;
    }
    int objID = this->registerObject(object, object->getHandle());
    // return nullptr if registration error occurs.
    return (objID == ID_UNDEFINED) ? nullptr : object;
  }  // postCreateRegister

  /**
   * @brief Get directory component of managed object handle and call @ref
   * esp::core::AbstractManagedObject::setFileDirectory if a legitimate
   * directory exists in handle.
   *
   * @param object pointer to managed object to set
   */
  void setFileDirectoryFromHandle(ManagedPtr object) {
    std::string handleName = object->getHandle();
    auto loc = handleName.find_last_of("/");
    if (loc != std::string::npos) {
      object->setFileDirectory(handleName.substr(0, loc));
    }
  }  // setFileDirectoryFromHandle

  /**
   * @brief Used Internally.  Create and configure newly-created managed object
   * with any default values, before any specific values are set.
   *
   * @param objectHandle handle name to be assigned to the managed object.
   * @param builtFromConfig Managed Object is being constructed from a config
   * file (i.e. @p objectHandle is config file filename).  If false this means
   * Manage Object is being constructed as some kind of new/default.
   * @return Newly created but unregistered ManagedObject pointer, with only
   * default values set.
   */
  virtual ManagedPtr initNewObjectInternal(const std::string& objectHandle,
                                           bool builtFromConfig) = 0;

  /**
   * @brief Used Internally. Remove the managed object referenced by the passed
   * string handle. Will emplace managed object ID within deque of usable IDs
   * and return the managed object being removed.
   * @param objectHandle the string key of the managed object desired.
   * @param src String denoting the source of the remove request.
   * @return the desired managed object being deleted, or nullptr if does not
   * exist
   */
  ManagedPtr removeObjectInternal(const std::string& objectHandle,
                                  const std::string& src);

  /**
   * @brief Used Internally. Get the ID of the managed object in @ref
   * objectLibrary_ for the given managed object Handle, if exists. If
   * the managed object is not in the library and getNext is true then returns
   * next available id, otherwise throws assertion and returns ID_UNDEFINED
   *
   * @param objectHandle The string key referencing the managed object in
   * @ref objectLibrary_. Usually the origin handle.
   * @param getNext Whether to get the next available ID if not found, or to
   * throw an assertion. Defaults to false
   * @return The managed object's ID if found. The next available ID if not
   * found and getNext is true. Otherwise ID_UNDEFINED.
   */
  int getObjectIDByHandleOrNew(const std::string& objectHandle, bool getNext) {
    if (getObjectLibHasHandle(objectHandle)) {
      return getObjectInternal<T>(objectHandle)->getID();
    } else {
      if (!getNext) {
        LOG(ERROR) << "ManagedContainerBase::getObjectIDByHandleOrNew : No "
                   << objectType_ << " managed object with handle "
                   << objectHandle << "exists. Aborting";
        return ID_UNDEFINED;
      } else {
        return getUnusedObjectID();
      }
    }
  }  // ManagedContainer::getObjectIDByHandle

  /**
   * @brief implementation of managed object type-specific registration
   * @param object the managed object to be registered
   * @param objectHandle the name to register the managed object with.
   * Expected to be valid.
   * @param forceRegistration Will register object even if conditional
   * registration checks fail.
   * @return The unique ID of the managed object being registered, or
   * ID_UNDEFINED if failed
   */
  virtual int registerObjectFinalize(ManagedPtr object,
                                     const std::string& objectHandle,
                                     bool forceRegistration) = 0;

  /**
   * @brief Build a shared pointer to a copy of a the passed managed object,
   * of appropriate managed object type for passed object type.
   * @tparam U Type of managed object being created - must be a derived class
   * of ManagedPtr
   * @param orig original object of type ManagedPtr being copied
   */
  template <typename U>
  ManagedPtr createObjectCopy(ManagedPtr& orig) {
    // don't call init on copy - assume copy is already properly initialized.
    return U::create(*(static_cast<U*>(orig.get())));
  }  // ManagedContainer::

  /**
   * @brief This function will build the appropriate @ref copyConstructorMap_
   * copy constructor function pointer map for this container's managed object,
   * keyed on the managed object's class type.  This MUST be called in the
   * constructor of the -instancing- class.
   */
  virtual void buildCtorFuncPtrMaps() = 0;

  /**
   * @brief Build an @ref esp::core::AbstractManagedObject object of type
   * associated with passed object.
   * @param origAttr The ptr to the original AbstractManagedObject object to
   * copy
   */
  ManagedPtr copyObject(ManagedPtr& origAttr) {
    const std::string ctorKey = origAttr->getClassKey();
    return (*this.*(this->copyConstructorMap_[ctorKey]))(origAttr);
  }  // ManagedContainer::copyObject

  /**
   * @brief Create a new object as a copy of @ref defaultObject_  if it exists,
   * otherwise return nullptr.
   * @param newHandle the name for the copy of the default.
   * @return New object or nullptr
   */
  ManagedPtr constructFromDefault(const std::string& newHandle) {
    if (defaultObj_ == nullptr) {
      return nullptr;
    }
    ManagedPtr res = this->copyObject(defaultObj_);
    if (nullptr != res) {
      res->setHandle(newHandle);
    }
    return res;
  }  // ManagedContainer::constructFromDefault

  /**
   * @brief add passed managed object to library, setting managedObjectID
   * appropriately. Called internally by registerObject.
   *
   * @param object the managed object to add to the library
   * @param objectHandle the origin handle/name of the managed object to
   * add. The origin handle of the managed object will be set to this
   * here, in case this managed object is constructed with a different handle.
   * @return the managedObjectID of the managed object
   */
  int addObjectToLibrary(ManagedPtr object, const std::string& objectHandle) {
    // set handle for managed object - might not have been set during
    // construction
    object->setHandle(objectHandle);
    // return either the ID of the existing managed object referenced by
    // objectHandle, or the next available ID if not found.
    int objectID = getObjectIDByHandleOrNew(objectHandle, true);
    object->setID(objectID);
    // make a copy of this managed object so that user can continue to edit
    // original
    ManagedPtr managedObjectCopy = copyObject(object);
    // add to libraries
    setObjectInternal(managedObjectCopy, objectHandle);
    objectLibKeyByID_.emplace(objectID, objectHandle);
    return objectID;
  }  // ManagedContainer::addObjectToLibrary

  // ======== Typedefs and Instance Variables ========

  /**
   * @brief Define a map type referencing function pointers to @ref
   * createObjectCopy keyed by string names of classes being instanced,
   */
  typedef std::map<std::string,
                   ManagedPtr (ManagedContainer<T>::*)(ManagedPtr&)>
      Map_Of_CopyCtors;

  /**
   * @brief Map of function pointers to instantiate a copy of a managed
   * object. A managed object is instanced by accessing the approrpiate
   * function pointer.
   */
  Map_Of_CopyCtors copyConstructorMap_;

  /**
   * @brief An object to provide default values, to be used upon
   * AbstractManagedObject construction
   */
  ManagedPtr defaultObj_ = nullptr;

 public:
  ESP_SMART_POINTERS(ManagedContainer<T>)

};  // class ManagedContainer

/////////////////////////////
// Class Template Method Definitions

template <class T>
auto ManagedContainer<T>::removeObjectsBySubstring(const std::string& subStr,
                                                   bool contains)
    -> std::vector<ManagedPtr> {
  std::vector<ManagedPtr> res;
  // get all handles that match query elements first
  std::vector<std::string> handles =
      getObjectHandlesBySubstring(subStr, contains);
  for (std::string objectHandle : handles) {
    ManagedPtr ptr = removeObjectInternal(
        objectHandle, "ManagedContainer::removeObjectsBySubstring");
    if (nullptr != ptr) {
      res.push_back(ptr);
    }
  }
  return res;
}  // ManagedContainer<T>::removeObjectsBySubstring

template <class T>
auto ManagedContainer<T>::removeObjectInternal(const std::string& objectHandle,
                                               const std::string& sourceStr)
    -> ManagedPtr {
  if (!checkExistsWithMessage(objectHandle, sourceStr)) {
    LOG(INFO) << sourceStr << " : Unable to remove " << objectType_
              << " managed object " << objectHandle << " : Does not exist.";
    return nullptr;
  }
  std::string msg;
  if (this->undeletableObjectNames_.count(objectHandle) > 0) {
    msg = "Required Undeletable Managed Object";
  } else if (this->userLockedObjectNames_.count(objectHandle) > 0) {
    msg = "User-locked Object.  To delete managed object, unlock it";
  }
  if (msg.length() != 0) {
    LOG(INFO) << sourceStr << " : Unable to remove " << objectType_
              << " managed object " << objectHandle << " : " << msg << ".";
    return nullptr;
  }

  ManagedPtr attribsTemplate = getObjectInternal<T>(objectHandle);
  // remove the object and all references to it from the various internal maps
  // holding them.
  deleteObjectInternal(attribsTemplate->getID(), objectHandle);
  return attribsTemplate;
}  // ManagedContainer::removeObjectInternal

}  // namespace core
}  // namespace esp

#endif  // ESP_CORE_MANAGEDCONTAINER_H_
