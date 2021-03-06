/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=78:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SpiderMonkey JavaScript engine.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef jsgc_statistics_h___
#define jsgc_statistics_h___

#include <string.h>

#include "jsfriendapi.h"
#include "jspubtd.h"
#include "jsutil.h"

struct JSCompartment;

namespace js {
namespace gcstats {

enum Phase {
    PHASE_GC_BEGIN,
    PHASE_WAIT_BACKGROUND_THREAD,
    PHASE_PURGE,
    PHASE_MARK,
    PHASE_MARK_ROOTS,
    PHASE_MARK_TYPES,
    PHASE_MARK_DELAYED,
    PHASE_MARK_OTHER,
    PHASE_FINALIZE_START,
    PHASE_SWEEP,
    PHASE_SWEEP_COMPARTMENTS,
    PHASE_SWEEP_OBJECT,
    PHASE_SWEEP_STRING,
    PHASE_SWEEP_SCRIPT,
    PHASE_SWEEP_SHAPE,
    PHASE_DISCARD_CODE,
    PHASE_DISCARD_ANALYSIS,
    PHASE_DISCARD_TI,
    PHASE_SWEEP_TYPES,
    PHASE_CLEAR_SCRIPT_ANALYSIS,
    PHASE_FINALIZE_END,
    PHASE_DESTROY,
    PHASE_GC_END,

    PHASE_LIMIT
};

enum Stat {
    STAT_NEW_CHUNK,
    STAT_DESTROY_CHUNK,

    STAT_LIMIT
};

class StatisticsSerializer;

struct Statistics {
    Statistics(JSRuntime *rt);
    ~Statistics();

    void beginPhase(Phase phase);
    void endPhase(Phase phase);

    void beginSlice(int collectedCount, int compartmentCount, gcreason::Reason reason);
    void endSlice();

    void reset(const char *reason) { slices.back().resetReason = reason; }
    void nonincremental(const char *reason) { nonincrementalReason = reason; }

    void count(Stat s) {
        JS_ASSERT(s < STAT_LIMIT);
        counts[s]++;
    }

    jschar *formatMessage();
    jschar *formatJSON(uint64_t timestamp);

  private:
    JSRuntime *runtime;

    int64_t startupTime;

    FILE *fp;
    bool fullFormat;

    /*
     * GCs can't really nest, but a second GC can be triggered from within the
     * JSGC_END callback.
     */
    int gcDepth;

    int collectedCount;
    int compartmentCount;
    const char *nonincrementalReason;

    struct SliceData {
        SliceData(gcreason::Reason reason, int64_t start)
          : reason(reason), resetReason(NULL), start(start)
        {
            PodArrayZero(phaseTimes);
            PodArrayZero(phaseFaults);
        }

        gcreason::Reason reason;
        const char *resetReason;
        int64_t start, end;
        int64_t phaseTimes[PHASE_LIMIT];
        size_t phaseFaults[PHASE_LIMIT];

        int64_t duration() const { return end - start; }
    };

    Vector<SliceData, 8, SystemAllocPolicy> slices;

    /* Most recent time when the given phase started. */
    int64_t phaseStartTimes[PHASE_LIMIT];
    size_t phaseStartFaults[PHASE_LIMIT];

    /* Total time in a given phase for this GC. */
    int64_t phaseTimes[PHASE_LIMIT];
    size_t phaseFaults[PHASE_LIMIT];

    /* Total time in a given phase over all GCs. */
    int64_t phaseTotals[PHASE_LIMIT];

    /* Number of events of this type for this GC. */
    unsigned int counts[STAT_LIMIT];

    /* Allocated space before the GC started. */
    size_t preBytes;

    void beginGC();
    void endGC();

    int64_t gcDuration();
    void printStats();
    bool formatData(StatisticsSerializer &ss, uint64_t timestamp);

    double computeMMU(int64_t resolution);
};

struct AutoGCSlice {
    AutoGCSlice(Statistics &stats, int collectedCount, int compartmentCount, gcreason::Reason reason
                JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : stats(stats)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
        stats.beginSlice(collectedCount, compartmentCount, reason);
    }
    ~AutoGCSlice() { stats.endSlice(); }

    Statistics &stats;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

struct AutoPhase {
    AutoPhase(Statistics &stats, Phase phase JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : stats(stats), phase(phase) { JS_GUARD_OBJECT_NOTIFIER_INIT; stats.beginPhase(phase); }
    ~AutoPhase() { stats.endPhase(phase); }

    Statistics &stats;
    Phase phase;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} /* namespace gcstats */
} /* namespace js */

#endif /* jsgc_statistics_h___ */
