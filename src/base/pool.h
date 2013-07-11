/*
 * Copyright (C) 2006-2013  Music Technology Group - Universitat Pompeu Fabra
 *
 * This file is part of Essentia
 *
 * Essentia is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License as published by the Free
 * Software Foundation (FSF), either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the Affero GNU General Public License
 * version 3 along with this program.  If not, see http://www.gnu.org/licenses/
 */

#ifndef ESSENTIA_POOL_H
#define ESSENTIA_POOL_H

#include "types.h"
#include "threading.h"
#include "tnt/tnt.h"
#include "essentiautil.h"

namespace essentia {

// standard map and not EssentiaMap because we want a new element
// to be automatically created when it doesn't exist
/** @todo use intel's tbb concurrent_map */
#define PoolOf(type) std::map<std::string, std::vector<type > >

typedef std::string DescriptorName;

/**
 * The pool is a storage structure which can hold frames of all kinds of
 * descriptors. A Pool instance is thread-safe.
 *
 * More specifically, a Pool maps descriptor names to data. A descriptor name
 * is a period ('.') delimited string of identifiers that are associated with
 * the values of some audio descriptor (or any other piece of data). For example, the descriptor
 * name "lowlevel.bpm" identifies a low-level value of beats per minute. Currently,
 * the Pool supports storing:
 *
 * - Reals
 * - Strings
 * - vectors of Reals
 * - vectors of Strings
 * - Array2D of Reals
 * - StereoSamples
 *
 * The Pool supports the ability to repeatedly add data under the same descriptor name as well as
 * associating a descriptor name with only one datum. The set function is used in the latter case,
 * and the former is explained in the next paragraph.
 *
 * When data is added to the pool under a given descriptor name, it is added to
 * a @b vector of data for that descriptor name. When the data is retrieved, a
 * @b vector of data which was stored under that descriptor name is returned.
 * For example, in the case of the descriptor name, "foo.bar", which maps to
 * Real descriptor values, every time a Real is added under the name
 * "foo.bar", it is actually stored in a vector. When the data mapped to by
 * "foo.bar" is retrieved, a @b vector of Reals is returned, even if only one
 * Real value was added under "foo.bar". In the case of data of type vector
 * of Reals, a @b vector @b of @b vector of Reals is returned when the data is
 * retrieved.
 *
 * It is not allowed to mix data types under the same descriptor name. Each of
 * the four types listed above are treated as separate types. In addition, a descriptor name that
 * maps to a single datum is considered mapping to a different type than a descriptor name that maps
 * to a @b vector of the same type.
 *
 * After a Pool is filled with data, it can be passed to the YamlOutput algorithm for saving the
 * data in the Pool to a file or it can be passed to the PoolAggregator algorithm for computing
 * statistics on the data in the Pool. Similarly, the file generated by YamlOutput can be restored
 * into a Pool using the YamlInput algorithm.
 *
 * For each type, the pool has its own public mutex (i.e. mutexReal, mutexVectorReal, etc.)
 * If locking the pool gobally or partially, lock should be acquired in the following order:
 *
 *         MutexLocker lockReal(mutexReal)
 *         MutexLocker lockVectorReal(mutexVectorReal)
 *         MutexLocker lockString(mutexString)
 *         MutexLocker lockVectorString(mutexVectorString)
 *         MutexLocker lockArray2DReal(mutexArray2DReal)
 *         MutexLocker lockStereoSample(mutexStereoSample)
 *         MutexLocker lockSingleReal(mutexSingleReal)
 *         MutexLocker lockSingleString(mutexSingleString)
 *         MutexLocker lockSingleVectorReal(mutexSingleVectorReal)
 *
 * To release the locks, the order should be reversed!
 *
 */
class Pool {

 protected:
  // maps for single values:
  std::map<std::string, Real> _poolSingleReal;
  std::map<std::string, std::string> _poolSingleString;
  std::map<std::string, std::vector<Real> > _poolSingleVectorReal;

  // maps for vectors of values:
  PoolOf(Real) _poolReal;
  PoolOf(std::vector<Real>) _poolVectorReal;
  PoolOf(std::string) _poolString;
  PoolOf(std::vector<std::string>) _poolVectorString;
  PoolOf(TNT::Array2D<Real>) _poolArray2DReal;
  PoolOf(StereoSample) _poolStereoSample;

  // WARNING: this function assumes that all sub-pools are locked
  std::vector<std::string> descriptorNamesNoLocking() const;

  /**
   * helper function for key validation when adding/setting/merging values to
   * the pool
   */
   void validateKey(const std::string& name);


 public:

  mutable Mutex mutexReal, mutexVectorReal, mutexString, mutexVectorString,
                mutexArray2DReal, mutexStereoSample,
                mutexSingleReal, mutexSingleString, mutexSingleVectorReal;

  /**
   * Adds @e value to the Pool under @e name
   * @param name a descriptor name that identifies the collection of data to add
   *             @e value to
   * @param value the value to add to the collection of data that @e name points
   *              to
   * @param validityCheck indicates whether @e value should be checked for NaN or Inf values. If
   *                      true, an exception is thrown if @e value is (or contains) a NaN or Inf.
   * @remark If @e name already exists in the pool and points to data with the
   *         same data type as @e value, then @e value is concatenated to the
   *         vector stored therein. If, however, @e name already exists in the
   *         pool and points to a @b different data type than @e value, then
   *         this can cause unwanted behavior for the rest of the member
   *         functions of Pool. To avoid this, do not add data into the Pool
   *         whose descriptor name already exists in the Pool and points to a
   *         @b different data type than @e value.
   *
   * @remark If @e name has child descriptor names, this function will throw an
   *         exception. For example, if "foo.bar" exists in the pool, this
   *         function can no longer be called with "foo" as its @e name
   *         parameter, because "bar" is a child descriptor name of "foo".
   */
  void add(const std::string& name, const Real& value, bool validityCheck = false);

  /** @copydoc add(const std::string&,const Real&,bool) */
  void add(const std::string& name, const std::vector<Real>& value, bool validityCheck = false);

  /** @copydoc add(const std::string&,const Real&,bool) */
  void add(const std::string& name, const std::string& value, bool validityCheck = false);

  /** @copydoc add(const std::string&,const Real&,bool) */
  void add(const std::string& name, const std::vector<std::string>& value, bool validityCheck = false);

  /** @copydoc add(const std::string&,const Real&,bool) */
  void add(const std::string& name, const TNT::Array2D<Real>& value, bool validityCheck = false);

  /** @copydoc add(const std::string&,const Real&,bool) */
  void add(const std::string& name, const StereoSample& value, bool validityCheck = false);

  /**
   * WARNING: this is an utility method that might fail in weird ways if not used
   * correctly. When in doubt, always use the add() method. This is provided for
   * optimization only.
   */
  template <typename T>
  void append(const std::string& name, const std::vector<T>& values);

  /**
   * \brief Sets the value of a descriptor name.
   *
   * \details This function is different than the add functions because set does not
   * append data to the existing data under a given descriptor name, it sets it.
   * Thus there can only be one datum associated with a descriptor name
   * introduced to the pool via the set function. This function is useful when
   * there is only one value associated with a given descriptor name.
   *
   * @param name is the descriptor name of the datum to set
   * @param value is the datum to associate with @e name
   *
   * @remark The set function cannot be used to override the data of a
   *         descriptor name that was introduced to the Pool via the add
   *         function. An EssentiaException will be thrown if the given
   *         descriptor name already exists in the pool and was put there via a
   *         call to an add function.
   */
  void set(const std::string& name, const Real& value, bool validityCheck=false);

  /** @copydoc set(const std::string&,const Real&, bool) */
  void set(const std::string& name, const std::vector<Real>& value, bool validityCheck=false);

  /** @copydoc set(const std::string&,const Real&i, bool) */
  void set(const std::string& name, const std::string& value, bool validityCheck=false);

  /**
   * \brief Merges the current pool with the given one @e p.
   *
   * \details If the pool contains a descriptor with name @e name, the current pool
   * will keep its original descriptor values unless a type of merging is
   * specified.
   *
   * Merge types can be:
   * - "replace" : if descriptor is not found it will be added to the pool otherwise
   *               it will remove the existing descriptor and subsitute it by
   *               the given one, regardless of type
   * - "append" : if descriptor is not found it will be added to the pool otherwise
   *              it will appended to the existing descriptor if and only ifthey share
   *              the same type.
   * - "interleave" : if descriptor is already in the pool, the new values will be
   *                  interleaved with the existing ones if and only if they
   *                  have the same type. If the descriptor is not found int
   *                  the pool it will be added.
   */

  void merge(Pool& p, const std::string& type="");

  /**
   * \brief Merges the values given in @e value into the current pool's
   * descriptor given by @e name.
   * @copydetails merge(Pool&, const std::string&)
   */
  void merge(const std::string& name, const std::vector<Real>& value, const std::string& type="");

  /** @copydoc merge(const std::string&, const std::vector<Real>&, const std::string&)*/
  void merge(const std::string& name, const std::vector<std::vector<Real> >& value, const std::string& type="");

  /** @copydoc merge(const std::string&, const std::vector<Real>&, const std::string&)*/
  void merge(const std::string& name, const std::vector<std::string>& value, const std::string& type="");

  /** @copydoc merge(const std::string&, const std::vector<Real>&, const std::string&)*/
  void merge(const std::string& name, const std::vector<std::vector<std::string> >& value, const std::string& type="");

  /** @copydoc merge(const std::string&, const std::vector<Real>&, const std::string&)*/
  void merge(const std::string& name, const std::vector<TNT::Array2D<Real> >& value, const std::string& type="");

  /** @copydoc merge(const std::string&, const std::vector<Real>&, const std::string&)*/
  void merge(const std::string& name, const std::vector<StereoSample>& value, const std::string& type="");

  /** @copydoc merge(const std::string&, const std::vector<Real>&, const std::string&)*/
  void mergeSingle(const std::string& name, const Real& value, const std::string& type="");
  /** @copydoc merge(const std::string&, const std::vector<Real>&, const std::string&)*/
  void mergeSingle(const std::string& name, const std::vector<Real>& value, const std::string& type="");
  /** @copydoc merge(const std::string&, const std::vector<Real>&, const std::string&)*/
  void mergeSingle(const std::string& name, const std::string& value, const std::string& type="");

  /**
   * Removes the descriptor name @e name from the Pool along with the data it
   * points to. This function does nothing if @e name does not exist in the
   * Pool.
   * @param name the descriptor name to remove
   */
  void remove(const std::string& name);

  /**
   * Removes the entire namespace given by @e ns from the Pool along with the
   * data itpoints to. This function does nothing if @e name does not exist in
   * the Pool.
   * @param name the descriptor name to remove
   */
  void removeNamespace(const std::string& ns);

  /**
   * @returns a the data that is associated with @e name
   * @param name is the descriptor name that points to the data to return
   * @tparam T is the type of data that @e name points to
   */
  template <typename T>
  const T& value(const std::string& name) const;

  /**
   * @returns whether the given descriptor name exists in the pool
   * @param the name of the descriptor you wish to check for
   * @tparam T is the type of data that @e name refers to
   */
  template <typename T>
  bool contains(const std::string& name) const;

  /**
   * @returns a vector containing all descriptor names in the Pool
   */
  std::vector<std::string> descriptorNames() const;

  /**
   * @returns a vector containing all descriptor names in the Pool which
   * belong to the specified namespace @e ns
   */
  std::vector<std::string> descriptorNames(const std::string& ns) const;

  /**
   * @returns a map where the key is a descriptor name and the values are
   *          of type Real
   */
  const PoolOf(Real)& getRealPool() const { return _poolReal; }

  /**
   * @returns a map where the key is a descriptor name and the values are
   *          of type vector<Real>
   */
  const PoolOf(std::vector<Real>)& getVectorRealPool() const { return _poolVectorReal; }

  /**
   * @returns a std::map where the key is a descriptor name and the values are
   *          of type string
   */
  const PoolOf(std::string)& getStringPool() const { return _poolString; }

  /**
   * @returns a std::map where the key is a descriptor name and the values are
   *          of type vector<string>
   */
  const PoolOf(std::vector<std::string>)& getVectorStringPool() const { return _poolVectorString; }

  /**
   * @returns a std::map where the key is a descriptor name and the values are
   *          of type TNT::Array2D<Real>
   */
  const PoolOf(TNT::Array2D<Real>)& getArray2DRealPool() const { return _poolArray2DReal; }

  /**
   * @returns a std::map where the key is a descriptor name and the values are
   *          of type StereoSample
   */
  const PoolOf(StereoSample)& getStereoSamplePool() const { return _poolStereoSample; }

  /**
   * @returns a std::map where the key is a descriptor name and the value is
   *          of type Real
   */
  const std::map<std::string, Real>& getSingleRealPool() const { return _poolSingleReal; }

  /**
   * @returns a std::map where the key is a descriptor name and the value is
   *          of type string
   */
  const std::map<std::string, std::string>& getSingleStringPool() const { return _poolSingleString; }

  /**
   * @returns a std::map where the key is a descriptor name and the value is
   *          of type vector<Real>
   */
  const std::map<std::string, std::vector<Real> >& getSingleVectorRealPool() const { return _poolSingleVectorReal; }

  /**
   * Checks that no descriptor name is in two different inner pool types at
   * the same time, and throws an EssentiaException if there is
   */
  void checkIntegrity() const;

  /**
   * Clears all the values contained in the pool.
   */
  void clear();

  /**
   * returns true if descriptor under name @e name is supposed to hold one
   * single value
   */
  bool isSingleValue(const std::string& name);
};


// make doxygen skip the macros
/// @cond

// T& Pool::value(const DescriptorName& name)
#define SPECIALIZE_VALUE(type, tname)                                          \
template <>                                                                    \
inline const type& Pool::value(const std::string& name) const {                \
  MutexLocker lock(mutex##tname);                                              \
  std::map<std::string,type >::const_iterator result = _pool##tname.find(name);\
  if (result == _pool##tname.end()) {                                          \
    std::ostringstream msg;                                                    \
    msg << "Descriptor name '" << name << "' of type "                         \
        << nameOfType(typeid(type)) << " not found";                           \
    throw EssentiaException(msg);                                              \
  }                                                                            \
  return result->second;                                                       \
}

SPECIALIZE_VALUE(Real, SingleReal);
SPECIALIZE_VALUE(std::string, SingleString);
SPECIALIZE_VALUE(std::vector<std::string>, String);
SPECIALIZE_VALUE(std::vector<std::vector<Real> >, VectorReal);
SPECIALIZE_VALUE(std::vector<std::vector<std::string> >, VectorString);
SPECIALIZE_VALUE(std::vector<TNT::Array2D<Real> >, Array2DReal);
SPECIALIZE_VALUE(std::vector<StereoSample>, StereoSample);

// This value function is not under the macro above because it needs to check
// in two separate sub-pools (poolReal and poolSingleVectorReal)
template<>
inline const std::vector<Real>& Pool::value(const std::string& name) const {
  std::map<std::string, std::vector<Real> >::const_iterator result;
  {
    MutexLocker lock(mutexReal);
    result = _poolReal.find(name);
    if (result != _poolReal.end()) {
      return result->second;
    }
  }

  {
    MutexLocker lock(mutexSingleVectorReal);
    result = _poolSingleVectorReal.find(name);
    if (result != _poolSingleVectorReal.end()) {
      return result->second;
    }
  }

  std::ostringstream msg;
  msg << "Descriptor name '" << name << "' of type "
      << nameOfType(typeid(std::vector<Real>)) << " not found";
  throw EssentiaException(msg);
}


// bool Pool::contains(const DescriptorName& name)
#define SPECIALIZE_CONTAINS(type, tname)                                       \
template <>                                                                    \
inline bool Pool::contains<type>(const std::string& name) const {              \
  MutexLocker lock(mutex##tname);                                              \
  std::map<std::string,type >::const_iterator result = _pool##tname.find(name);\
  if (result == _pool##tname.end()) {                                          \
    return false;                                                              \
  }                                                                            \
  return true;                                                                 \
}

SPECIALIZE_CONTAINS(Real, SingleReal);
SPECIALIZE_CONTAINS(std::string, SingleString);
SPECIALIZE_CONTAINS(std::vector<std::string>, String);
SPECIALIZE_CONTAINS(std::vector<std::vector<Real> >, VectorReal);
SPECIALIZE_CONTAINS(std::vector<std::vector<std::string> >, VectorString);
SPECIALIZE_CONTAINS(std::vector<TNT::Array2D<Real> >, Array2DReal);
SPECIALIZE_CONTAINS(std::vector<StereoSample>, StereoSample);

// This value function is not under the macro above because it needs to check
// in two separate sub-pools (poolReal and poolSingleVectorReal)
template<>
inline bool Pool::contains<std::vector<Real> >(const std::string& name) const {
  std::map<std::string, std::vector<Real> >::const_iterator result;
  {
    MutexLocker lock(mutexReal);
    result = _poolReal.find(name);
    if (result != _poolReal.end()) {
      return true;
    }
  }

  {
    MutexLocker lock(mutexSingleVectorReal);
    result = _poolSingleVectorReal.find(name);
    if (result != _poolSingleVectorReal.end()) {
      return true;
    }
  }

  return false;
}


// Used to get a lock over all sub-pools, make sure to update this when adding
// a new sub-pool
#define GLOBAL_LOCK                                         \
MutexLocker lockReal(mutexReal);                            \
MutexLocker lockVectorReal(mutexVectorReal);                \
MutexLocker lockString(mutexString);                        \
MutexLocker lockVectorString(mutexVectorString);            \
MutexLocker lockArray2DReal(mutexArray2DReal);              \
MutexLocker lockStereoSample(mutexStereoSample);            \
MutexLocker lockSingleReal(mutexSingleReal);                \
MutexLocker lockSingleString(mutexSingleString);            \
MutexLocker lockSingleVectorReal(mutexSingleVectorReal);




template<typename T>
inline void Pool::append(const std::string& name, const std::vector<T>& values) {
  throw EssentiaException("Pool::append not implemented for type: ", nameOfType(typeid(T)));
}

#define SPECIALIZE_APPEND(type, tname)                                                \
template <>                                                                           \
inline void Pool::append(const std::string& name, const std::vector<type>& values) {  \
  {                                                                                   \
    MutexLocker lock(mutex##tname);                                                   \
    PoolOf(type)::iterator result = _pool##tname.find(name);                          \
    if (result != _pool##tname.end()) {                                               \
                                                                                      \
      std::vector<type>& v = result->second;                                          \
      int vsize = v.size();                                                           \
      v.resize(vsize + values.size());                                                \
      fastcopy(&v[vsize], &values[0], values.size());                                 \
      return;                                                                         \
    }                                                                                 \
  }                                                                                   \
                                                                                      \
  GLOBAL_LOCK                                                                         \
  validateKey(name);                                                                  \
  _pool##tname[name] = values;                                                        \
}


SPECIALIZE_APPEND(Real, Real);
SPECIALIZE_APPEND(std::vector<Real>, VectorReal);
SPECIALIZE_APPEND(std::string, String);
SPECIALIZE_APPEND(std::vector<std::string>, VectorString);
SPECIALIZE_APPEND(StereoSample, StereoSample);

/// @endcond

} // namespace essentia

#endif // ESSENTIA_POOL_H