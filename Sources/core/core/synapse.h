#pragma once

#include <cstdint>
#include <tuple>
#include <vector>

#include <tbb/spin_mutex.h>
#include <tbb/scalable_allocator.h>

#include <pup.h>

#include "common.h"

class Synapse
{
public:
    typedef tbb::spin_mutex::scoped_lock Lock;

    enum class Type : std::uint8_t
    {
        Empty = 0,
        Weighted = 1,
        Lagging = 2,
        Conductive = 3,
        Probabilistic = 4
    };

    struct Data
    {
        Data();
        Data(const Data &other);
        Data(const Data &&other);
        Data &operator=(const Data &other);
        Data &operator=(Data &&other);

        void pup(PUP::er &p);

        Type type;
        tbb::spin_mutex lock;
        uint16_t bits16;
        uint64_t bits64;
    };

    typedef std::tuple<Direction, NeuronId, NeuronId, Data> Addition;
    typedef std::tuple<Direction, NeuronId, NeuronId> Removal;
    typedef std::pair<NeuronId, NeuronId> Transfer;

    class Editor
    {
    public:
        virtual ~Editor() = default;

        virtual size_t ExtraBytes() const;
        virtual void *AllocateExtra();

        virtual void Initialize(Data &data);
        virtual void Clone(const Data &original, Data &data);
        virtual void Release(Data &data);
    };

    class Accessor
    {
    public:
        friend class Synapse;

        Data &GetData();
        Editor *GetEditor();

    protected:
        void Set(Editor *editor, Data &data, bool doLock);

    private:
        Lock mLock;
        Data *mData;
        Editor *mEditor;
    };

    static Type GetType(const Data &data);
    static void Initialize(Type type, Data &data);
    static void Clone(const Data &original, Data &data);
    static Editor *Edit(Data &data, Accessor &accessor, bool doLock = true);
    static void Release(Data &data);

private:
    Synapse();
    Synapse(const Synapse &other) = delete;
    Synapse &operator=(const Synapse &other) = delete;

    static Synapse instance;

    std::vector<std::unique_ptr<Editor>> mEditors;
};

class WeightedSynapse : public Synapse::Editor
{
public:
    virtual void Initialize(Synapse::Data &data) override;
    virtual void Clone(const Synapse::Data &original, Synapse::Data &data) override;

    double GetWeight(const Synapse::Data &data) const;
    void SetWeight(Synapse::Data &data, double weight);
};

class LaggingSynapse : public Synapse::Editor
{
public:
    virtual void Initialize(Synapse::Data &data) override;
    virtual void Clone(const Synapse::Data &original, Synapse::Data &data) override;

    double GetWeight(const Synapse::Data &data) const;
    void SetWeight(Synapse::Data &data, double weight);
    uint16_t GetDelay(const Synapse::Data &data) const;
    void SetDelay(Synapse::Data &data, uint16_t delay);
};

class ConductiveSynapse : public Synapse::Editor
{
public:
    virtual void Initialize(Synapse::Data &data) override;
    virtual void Clone(const Synapse::Data &original, Synapse::Data &data) override;

    float GetWeight(const Synapse::Data &data) const;
    void SetWeight(Synapse::Data &data, float weight);
    uint16_t GetDelay(const Synapse::Data &data) const;
    void SetDelay(Synapse::Data &data, uint16_t delay);
    float GetConductance(const Synapse::Data &data) const;
    void SetConductance(Synapse::Data &data, float conductance);

protected:
    static const uint8_t WeightOffset = 0;
    static const uint64_t WeightMask = 0x00000000FFFFFFFF;
    static const uint8_t ConductanceOffset = 32;
    static const uint64_t ConductanceMask = 0xFFFFFFFF00000000;
};

class ProbabilisticSynapse : public Synapse::Editor
{
public:
    virtual size_t ExtraBytes() const override;
    virtual void *AllocateExtra() override;

    virtual void Initialize(Synapse::Data &data) override;
    virtual void Clone(const Synapse::Data &original, Synapse::Data &data) override;
    virtual void Release(Synapse::Data &data) override;

    double GetWeight(const Synapse::Data &data) const;
    void SetWeight(Synapse::Data &data, double weight);
    uint16_t GetDelay(const Synapse::Data &data) const;
    void SetDelay(Synapse::Data &data, uint16_t delay);
    float GetMean(const Synapse::Data &data) const;
    void SetMean(Synapse::Data &data, float mean);
    float GetVariance(const Synapse::Data &data) const;
    void SetVariance(Synapse::Data &data, float variance);

protected:
    struct DataExtended
    {
        DataExtended();

        float mean;
        float variance;
        double weight;
    };

private:
    tbb::scalable_allocator<DataExtended> mAllocator;
};