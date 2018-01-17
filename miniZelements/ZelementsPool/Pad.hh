#ifndef PAD_H
#define PAD_H

#include "ZTypes.hh"
#include "zinc/debug.hh"

#include <string>
#include <boost/shared_ptr.hpp>
#include <iostream>
#include <zinc/evt.hh>

namespace mfw { namespace zel {

/*!
* \brief This class represents a junction of two zelements.
*
*    it allows data exchange between zelements
*    only single to single to sigle connectin is allowed
*/
class IZelement;
class Pad {
public:
    typedef std::vector<Pad*>::iterator PadItr;
    typedef boost::shared_ptr<evt::Event> SynchEvt;

    Pad ( ) : m_DataType(ANY) {/*EMPTY*/};

    virtual ~Pad ( ) {/*EMPTY*/};

   /*!
    * \brief Together with Delegate it constitues delegate system
    *
    *    only here because method function pointers are brokenin C++
    */
    class DelegateBase {
    public:
        virtual ssize_t operator()(char *, size_t, SynchEvt*) =0;
        virtual char* operator()(size_t) =0;
        DelegateBase() : m_FathersPad(NULL), m_ZelFather(NULL) {/*EMPTY*/};

        virtual ~DelegateBase() {/*EMPTY*/};
        Pad *GetFathersPad() {
            return m_FathersPad;
        }
        IZelement *GetFather() {
            return m_ZelFather;
        }
    protected:
        Pad *m_FathersPad;
        IZelement *m_ZelFather;
    };

    template <typename T>
    class Delegate : public DelegateBase {
    public:
        char* operator()(size_t a_Size) { 
            (void)a_Size;
            return 0; // dummy for Delegate, but must declare because DelegateA implements it
        };
        ssize_t operator()(char *a_Data, size_t a_DataLen, SynchEvt *a_MetaData) {
            return (m_Father->*m_Mfp)(a_Data, a_DataLen, a_MetaData, m_FathersPad);
        }
        Delegate(T *a_T, ssize_t (T::*a_Mfp)(char *, size_t, SynchEvt*, Pad*), Pad *a_FathersPad = NULL) : m_Father(a_T), m_Mfp(a_Mfp) {
            m_FathersPad = a_FathersPad;
            m_ZelFather = dynamic_cast<IZelement*>(m_Father);
        }
        virtual ~Delegate() {/*EMPTY*/};
    private:
        T *m_Father;
        ssize_t (T::*m_Mfp)(char *, size_t, SynchEvt*, Pad*);
    };
    template <typename T>
    class DelegateA : public DelegateBase {
    public:
        char* operator()(size_t a_Size) {
            return (m_Father->*m_Mfp)(a_Size, m_FathersPad);
        };
        ssize_t operator()(char *a_Data, size_t a_DataLen, SynchEvt *a_MetaData) { 
            return 0; // dummy for DelegateA, but must declare because Delegate implements it
        };
        DelegateA(T *a_T, char* (T::*a_Mfp)(size_t, Pad*), Pad *a_FathersPad = NULL) : m_Father(a_T), m_Mfp(a_Mfp) {
            m_FathersPad = a_FathersPad;
        }
        virtual ~DelegateA() {/*EMPTY*/};
    private:
        T *m_Father;
        char* (T::*m_Mfp)(size_t, Pad*);
    };

    /*!
    * \brief Connects two pads together so data can be exchanged between them
    *
    */
    virtual RCode ConnectInput(const Pad* a_Pad) {
        if (GetDataType() != const_cast<Pad*>(a_Pad)->GetDataType() &&
            GetDataType() != ANY &&
            const_cast<Pad*>(a_Pad)->GetDataType() != ANY
            ) {
            LOG_ERROR(("Can connect only pads exchanging the same type of data!"));
            return NOK;
        }
        m_Write = a_Pad->m_Write;
        m_Alloc = a_Pad->m_Alloc;
        return OK;
    }

    /*!
    * \brief Set type of data which can be exchanged via this pad
    *
    */
    void SetDataType (DataType a_DataType ) {
        m_DataType = a_DataType;
    }


    DataType GetDataType ( ) {
        return m_DataType;
    }

    DelegateBase *SetRead(DelegateBase *a_Read) {
        boost::shared_ptr<DelegateBase> p(a_Read);
        m_Read = p;
        return a_Read;
    }

    DelegateBase *SetWrite(DelegateBase *a_Write) {
        boost::shared_ptr<DelegateBase> p(a_Write);
        m_Write = p;
        return a_Write;
    }

    DelegateBase *SetAllocate(DelegateBase *a_Allocate) {
        boost::shared_ptr<DelegateBase> p(a_Allocate);
        m_Alloc = p;
        return a_Allocate;
    }

    ssize_t Read(char *a_Data, size_t a_DataLen, SynchEvt *a_MetaData) {
        return m_Read.get() == NULL ? -1 : (*m_Read)(a_Data, a_DataLen, a_MetaData);
    }
    
    ssize_t Write(char *a_Data, size_t a_DataLen, SynchEvt *a_MetaData) {
        return m_Write.get() == NULL ? -1 : (*m_Write)(a_Data, a_DataLen, a_MetaData);
    }

    char *Allocate(size_t a_Size) {
        return (m_Alloc.get() == NULL ) ? NULL : (*m_Alloc)(a_Size);
    }
    IZelement *GetFather() const {
        if (m_Write.get() != NULL) {
            return m_Write->GetFather();
        }else if (m_Read.get() != NULL) {
            return m_Read->GetFather();
        }else {
        //orphant ?!
            return NULL;
        }
    }
    Pad *GetFathersPad() const {
        if (m_Write.get() != NULL) {
            return m_Write->GetFathersPad();
        }else if (m_Read.get() != NULL) {
            return m_Read->GetFathersPad();
        }else {
        //orphant ?!
            return NULL;
        }
    }
private:
    boost::shared_ptr<DelegateBase> m_Read;//keeps reference to read method(delegate)
    boost::shared_ptr<DelegateBase> m_Write;//keeps reference to write method(delegate)
    boost::shared_ptr<DelegateBase> m_Alloc;//keeps reference to alloc method(delegate)
    DataType m_DataType;//stores type of data which can be exchanged via this pad
};

};};

#endif // PAD_H
