//===-- TraceIntelPT.h ------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLDB_SOURCE_PLUGINS_TRACE_INTEL_PT_TRACEINTELPT_H
#define LLDB_SOURCE_PLUGINS_TRACE_INTEL_PT_TRACEINTELPT_H

#include "TaskTimer.h"
#include "ThreadDecoder.h"
#include "TraceIntelPTMultiCpuDecoder.h"
#include "TraceIntelPTBundleLoader.h"

#include "lldb/Utility/FileSpec.h"
#include "lldb/lldb-types.h"
#include "llvm/Support/raw_ostream.h"

namespace lldb_private {
namespace trace_intel_pt {

class TraceIntelPT : public Trace {
public:
  void Dump(Stream *s) const override;

  llvm::Error SaveLiveTraceToDisk(FileSpec directory) override;

  ~TraceIntelPT() override = default;

  /// PluginInterface protocol
  /// \{
  llvm::StringRef GetPluginName() override { return GetPluginNameStatic(); }

  static void Initialize();

  static void Terminate();

  /// Create an instance of this class from a trace bundle.
  ///
  /// \param[in] trace_bundle_description
  ///     The description of the trace bundle. See \a Trace::FindPlugin.
  ///
  /// \param[in] bundle_dir
  ///     The path to the directory that contains the trace bundle.
  ///
  /// \param[in] debugger
  ///     The debugger instance where new Targets will be created as part of the
  ///     JSON data parsing.
  ///
  /// \return
  ///     A trace instance or an error in case of failures.
  static llvm::Expected<lldb::TraceSP>
  CreateInstanceForTraceBundle(const llvm::json::Value &trace_bundle_description,
                               llvm::StringRef bundle_dir,
                               Debugger &debugger);

  static llvm::Expected<lldb::TraceSP>
  CreateInstanceForLiveProcess(Process &process);

  static llvm::StringRef GetPluginNameStatic() { return "intel-pt"; }
  /// \}

  lldb::CommandObjectSP
  GetProcessTraceStartCommand(CommandInterpreter &interpreter) override;

  lldb::CommandObjectSP
  GetThreadTraceStartCommand(CommandInterpreter &interpreter) override;

  llvm::StringRef GetSchema() override;

  lldb::TraceCursorUP GetCursor(Thread &thread) override;

  void DumpTraceInfo(Thread &thread, Stream &s, bool verbose) override;

  llvm::Expected<llvm::Optional<uint64_t>> GetRawTraceSize(Thread &thread);

  llvm::Error DoRefreshLiveProcessState(TraceGetStateResponse state,
                                        llvm::StringRef json_response) override;

  bool IsTraced(lldb::tid_t tid) override;

  const char *GetStartConfigurationHelp() override;

  /// Start tracing a live process.
  ///
  /// More information on the parameters below can be found in the
  /// jLLDBTraceStart section in lldb/docs/lldb-gdb-remote.txt.
  ///
  /// \param[in] ipt_trace_size
  ///     Trace size per thread in bytes.
  ///
  /// \param[in] total_buffer_size_limit
  ///     Maximum total trace size per process in bytes.
  ///
  /// \param[in] enable_tsc
  ///     Whether to use enable TSC timestamps or not.
  ///
  /// \param[in] psb_period
  ///     This value defines the period in which PSB packets will be generated.
  ///
  /// \param[in] per_cpu_tracing
  ///     This value defines whether to have an intel pt trace buffer per thread
  ///     or per cpu core.
  ///
  /// \return
  ///     \a llvm::Error::success if the operation was successful, or
  ///     \a llvm::Error otherwise.
  llvm::Error Start(uint64_t ipt_trace_size, uint64_t total_buffer_size_limit,
                    bool enable_tsc, llvm::Optional<uint64_t> psb_period,
                    bool m_per_cpu_tracing);

  /// \copydoc Trace::Start
  llvm::Error Start(StructuredData::ObjectSP configuration =
                        StructuredData::ObjectSP()) override;

  /// Start tracing live threads.
  ///
  /// More information on the parameters below can be found in the
  /// jLLDBTraceStart section in lldb/docs/lldb-gdb-remote.txt.
  ///
  /// \param[in] tids
  ///     Threads to trace.
  ///
  /// \param[in] ipt_trace_size
  ///     Trace size per thread or per cpu core in bytes.
  ///
  /// \param[in] enable_tsc
  ///     Whether to use enable TSC timestamps or not.
  ///
  /// \param[in] psb_period
  ///     This value defines the period in which PSB packets will be generated.
  ///
  /// \return
  ///     \a llvm::Error::success if the operation was successful, or
  ///     \a llvm::Error otherwise.
  llvm::Error Start(llvm::ArrayRef<lldb::tid_t> tids, uint64_t ipt_trace_size,
                    bool enable_tsc, llvm::Optional<uint64_t> psb_period);

  /// \copydoc Trace::Start
  llvm::Error Start(llvm::ArrayRef<lldb::tid_t> tids,
                    StructuredData::ObjectSP configuration =
                        StructuredData::ObjectSP()) override;

  /// See \a Trace::OnThreadBinaryDataRead().
  llvm::Error OnThreadBufferRead(lldb::tid_t tid,
                                 OnBinaryDataReadCallback callback);

  /// Get or fetch the cpu information from, for example, /proc/cpuinfo.
  llvm::Expected<pt_cpu> GetCPUInfo();

  /// Get or fetch the values used to convert to and from TSCs and nanos.
  llvm::Optional<LinuxPerfZeroTscConversion> GetPerfZeroTscConversion();

  /// \return
  ///     The timer object for this trace.
  TaskTimer &GetTimer();

  TraceIntelPTSP GetSharedPtr();

private:
  friend class TraceIntelPTBundleLoader;

  llvm::Expected<pt_cpu> GetCPUInfoForLiveProcess();

  /// Postmortem trace constructor
  ///
  /// \param[in] bundle_description
  ///     The definition file for the postmortem bundle.
  ///
  /// \param[in] traced_processes
  ///     The processes traced in the live session.
  ///
  /// \param[in] trace_threads
  ///     The threads traced in the live session. They must belong to the
  ///     processes mentioned above.
  ///
  /// \return
  ///     A TraceIntelPT shared pointer instance.
  /// \{
  static TraceIntelPTSP CreateInstanceForPostmortemTrace(
      JSONTraceBundleDescription &bundle_description,
      llvm::ArrayRef<lldb::ProcessSP> traced_processes,
      llvm::ArrayRef<lldb::ThreadPostMortemTraceSP> traced_threads);

  /// This constructor is used by CreateInstanceForPostmortemTrace to get the
  /// instance ready before using shared pointers, which is a limitation of C++.
  TraceIntelPT(JSONTraceBundleDescription &bundle_description,
               llvm::ArrayRef<lldb::ProcessSP> traced_processes);
  /// \}

  /// Constructor for live processes
  TraceIntelPT(Process &live_process) : Trace(live_process){};

  /// Decode the trace of the given thread that, i.e. recontruct the traced
  /// instructions.
  ///
  /// \param[in] thread
  ///     If \a thread is a \a ThreadTrace, then its internal trace file will be
  ///     decoded. Live threads are not currently supported.
  ///
  /// \return
  ///     A \a DecodedThread shared pointer with the decoded instructions. Any
  ///     errors are embedded in the instruction list.
  DecodedThreadSP Decode(Thread &thread);

  /// We package all the data that can change upon process stops to make sure
  /// this contract is very visible.
  /// This variable should only be accessed directly by constructores or live
  /// process data refreshers.
  struct Storage {
    llvm::Optional<TraceIntelPTMultiCpuDecoder> multicpu_decoder;
    /// These decoders are used for the non-per-cpu case
    llvm::DenseMap<lldb::tid_t, std::unique_ptr<ThreadDecoder>> thread_decoders;
    /// Helper variable used to track long running operations for telemetry.
    TaskTimer task_timer;
    /// It is provided by either a trace bundle or a live process to convert TSC
    /// counters to and from nanos. It might not be available on all hosts.
    llvm::Optional<LinuxPerfZeroTscConversion> tsc_conversion;
  } m_storage;

  /// It is provided by either a trace bundle or a live process' "cpuInfo"
  /// binary data. We don't put it in the Storage because this variable doesn't
  /// change.
  llvm::Optional<pt_cpu> m_cpu_info;

  /// Get the storage after refreshing the data in the case of a live process.
  Storage &GetUpdatedStorage();
};

} // namespace trace_intel_pt
} // namespace lldb_private

#endif // LLDB_SOURCE_PLUGINS_TRACE_INTEL_PT_TRACEINTELPT_H
