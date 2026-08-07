// SPDX-License-Identifier: GPL-3.0-or-later

#include "reassembly.h"

namespace amnezia::qqdns {

DataHandler::DataHandler(int offsetsSize, qint64 assembleMs)
    : m_assembleMs(assembleMs), m_slots(offsetsSize)
{
}

DataHandler::Result DataHandler::newDataEvent(quint32 key, int fragmentPart, bool lastFragment,
                                              const QByteArray &data, qint64 nowMs)
{
    Result result;
    if (fragmentPart < 0 || fragmentPart >= kNumSlots) {
        return result;
    }
    if (key >= static_cast<quint32>(m_slots.size())) {
        return result;
    }
    Slot &slot = m_slots[static_cast<int>(key)];

    // Lazy expiry: a stale key behaves as Empty (offset reuse after wrap).
    if (slot.kind != Kind::Empty && (nowMs - slot.createdAt) >= m_assembleMs) {
        slot.kind = Kind::Empty;
        slot.asm_.reset();
    }

    switch (slot.kind) {
    case Kind::Done:
    case Kind::Rejected:
        return result;

    case Kind::Empty: {
        const int biggestIndexPlusOne = fragmentPart + 1;
        // Single-fragment datagram: fragment 0 flagged last.
        if (lastFragment && biggestIndexPlusOne == 1) {
            slot.kind = Kind::Done;
            slot.createdAt = nowMs;
            slot.asm_.reset();
            result.complete = true;
            result.joined = data;
            return result;
        }
        slot.kind = Kind::Assembling;
        slot.createdAt = nowMs;
        slot.asm_ = std::make_unique<Assembly>();
        slot.asm_->parts[fragmentPart] = data;
        slot.asm_->present[fragmentPart] = true;
        slot.asm_->recNums = 1;
        slot.asm_->biggestIndexPlusOne = biggestIndexPlusOne;
        slot.asm_->seenLastFragment = lastFragment;
        return result;
    }

    case Kind::Assembling: {
        Assembly &a = *slot.asm_;
        if (a.present[fragmentPart]) {
            return result; // duplicate fragment
        }
        a.parts[fragmentPart] = data;
        a.present[fragmentPart] = true;
        const int recNums = a.recNums + 1;
        const int fpPo = fragmentPart + 1;
        const int pBiggest = a.biggestIndexPlusOne;
        bool biggestUpdated;
        int biggestIndexPlusOne;
        if (fpPo > pBiggest) {
            biggestUpdated = true;
            biggestIndexPlusOne = fpPo;
        } else {
            biggestUpdated = false;
            biggestIndexPlusOne = pBiggest;
        }
        const bool pSeenLast = a.seenLastFragment;

        // Inconsistent: a second "last", a fragment past the seen last, or a
        // "last" that is not the highest index. Poison the key.
        if ((lastFragment && pSeenLast) || (biggestUpdated && pSeenLast)
            || (!biggestUpdated && lastFragment)) {
            slot.kind = Kind::Rejected;
            slot.createdAt = nowMs;
            slot.asm_.reset();
            return result;
        }

        const bool seenLast = lastFragment || pSeenLast;
        if (seenLast && recNums == biggestIndexPlusOne) {
            // Complete: concatenate fragments 0..recNums in order.
            QByteArray joined;
            for (int i = 0; i < recNums; ++i) {
                joined.append(a.parts[i]);
            }
            slot.kind = Kind::Done;
            slot.createdAt = nowMs;
            slot.asm_.reset();
            result.complete = true;
            result.joined = joined;
            return result;
        }

        a.recNums = recNums;
        if (lastFragment) {
            a.seenLastFragment = true;
        }
        if (biggestUpdated) {
            a.biggestIndexPlusOne = biggestIndexPlusOne;
        }
        return result;
    }
    }

    return result;
}

void DataHandler::sweep(qint64 nowMs)
{
    for (Slot &slot : m_slots) {
        if (slot.kind != Kind::Empty && (nowMs - slot.createdAt) >= m_assembleMs) {
            slot.kind = Kind::Empty;
            slot.asm_.reset();
        }
    }
}

} // namespace amnezia::qqdns
