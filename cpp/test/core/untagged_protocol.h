#pragma once

#include <bond/core/null.h>
#include <bond/protocol/simple_binary.h>

#include <stack>

// An implementation of untagged protocol supporting omitting optional fields.
// It inherits most of the encoding from SimpleBinary and prefixes each 8 fields
// with an 8-bit bitmap with omitted fields marked with bit 1.

template <typename Buffer>
class UntaggedProtocolWriter;

template <typename Buffer>
class UntaggedProtocolReader
    : public bond::SimpleBinaryReader<Buffer>
{
public:
    typedef bond::StaticParser<UntaggedProtocolReader&> Parser;
    typedef UntaggedProtocolWriter<Buffer>              Writer;
    
    UntaggedProtocolReader(typename boost::call_traits<Buffer>::param_type input)
        : bond::SimpleBinaryReader<Buffer>(input)
    {}

    using bond::SimpleBinaryReader<Buffer>::Skip;

    template <typename T>
    void Skip(const bond::bonded<T, UntaggedProtocolReader&>& bonded)
    {
        // Skip the structure field-by-field by applying Null transform
        Apply(bond::Null(), bonded);
    }

    // ReadStructBegin
    void ReadStructBegin()
    {
        _stack.push(Bitmap());
    }

    // ReadStructEnd
    void ReadStructEnd()
    {
        _stack.pop();
    }

    // ReadFieldOmitted
    bool ReadFieldOmitted()
    {
        Bitmap& my = _stack.top();

        if (my.bit == 8)
        {
            this->Read(my.bitmap);
            my.bit = 0;
        }

        return !!(my.bitmap & (1 << my.bit++));
    }

private:
    struct Bitmap
    {
        Bitmap()
            : bit(8)
        {}

        uint8_t bitmap;
        uint8_t bit;
    };

    std::stack<Bitmap> _stack;
};


template <typename Buffer>
class UntaggedProtocolWriter
    : public bond::SimpleBinaryWriter<Buffer>
{
public:
    typedef UntaggedProtocolReader<Buffer> Reader;

    UntaggedProtocolWriter(Buffer& output)
        : bond::SimpleBinaryWriter<Buffer>(output)
    {}

    void WriteStructBegin(const bond::Metadata& /*metadata*/, bool /*base*/)
    {
        _stack.push(Bitmap());
    }

    void WriteStructEnd(bool = false)
    {
        Bitmap& my = _stack.top();

        if (my.bit != 0)
            my.placeholder.Write(my.bitmap);
        
        _stack.pop();
    }

    // WriteFieldBegin
    void WriteFieldBegin(bond::BondDataType /*type*/, uint16_t /*id*/)
    {
        SetFieldBit(false);
    }

    void WriteFieldBegin(bond::BondDataType /*type*/, uint16_t /*id*/, const bond::Metadata& /*metadata*/)
    {
        SetFieldBit(false);
    }
    
    // WriteFieldOmitted
    void WriteFieldOmitted(bond::BondDataType /*type*/, uint16_t /*id*/, const bond::Metadata& /*metadata*/)
    {
        SetFieldBit(true);
    }

    template <typename T>
    void WriteField(uint16_t /*id*/, const bond::Metadata& /*metadata*/, const T& value)
    {
        SetFieldBit(false);
        this->Write(value);
    }

private:
    void SetFieldBit(bool bit)
    {
        Bitmap& my = _stack.top();

        if (my.bit == 0)
        {
            // Snap a copy of the output stream at current point. 
            // Note that this trick only works with OutputBuffer if the memory is preallocated.
            my.placeholder = this->_output;
            // Write placeholder for the bitmap
            this->Write(my.bitmap = 0);
        }

        my.bitmap |= (bit ? 1 : 0) << my.bit++;

        if (my.bit == 8)
        {
            // Write bitmap to the placeholder
            my.placeholder.Write(my.bitmap);
            // Next field will start new bitmap
            my.bit = 0;
        }
    }

    struct Bitmap
    {
        Bitmap()
            : bit(0)
        {}

        uint8_t bitmap;
        uint8_t bit;
        Buffer  placeholder;
    };

    std::stack<Bitmap> _stack;
};


