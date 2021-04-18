/** \file
 * Defines the sweep point writer pass.
 */

#include "ql/pass/io/sweep_points/write.h"

#include "ql/utils/filesystem.h"
#include "ql/pass/io/sweep_points/annotation.h"

namespace ql {
namespace pass {
namespace io {
namespace sweep_points {
namespace write {

/**
 * Dumps docs for the sweep point writer.
 */
void WriteSweepPointsPass::dump_docs(
    std::ostream &os,
    const utils::Str &line_prefix
) const {
    utils::dump_str(os, line_prefix, R"(
    TODO
    )");
}

/**
 * Constructs a sweep point writer.
 */
WriteSweepPointsPass::WriteSweepPointsPass(
    const utils::Ptr<const pmgr::Factory> &pass_factory,
    const utils::Str &instance_name,
    const utils::Str &type_name
) : pmgr::pass_types::ProgramAnalysis(pass_factory, instance_name, type_name) {
}

/**
 * Runs the sweep point writer.
 */
utils::Int WriteSweepPointsPass::run(
    const ir::ProgramRef &program,
    const pmgr::pass_types::Context &context
) const {

    QL_DOUT("write_sweep_points()");
    auto annot = program->get_annotation_ptr<Annotation>();
    if (annot != nullptr && annot->data.size()) {
        utils::StrStrm ss_swpts;
        ss_swpts << "{ \"measurement_points\" : [";
        for (utils::UInt i = 0; i < annot->data.size() - 1; i++) {
            ss_swpts << annot->data[i] << ", ";
        }
        ss_swpts << annot->data[annot->data.size() - 1] << "] }";
        utils::Str config = ss_swpts.str();

        utils::Str conf_file_name;
        if (annot->config_file_name.empty()) {
            conf_file_name = context.output_prefix + ".json";
        } else {
            conf_file_name = annot->config_file_name;
        }

        QL_IOUT("writing sweep points to '" << conf_file_name << "'...");
        utils::OutFile(conf_file_name).write(config);
    } else {
        QL_IOUT("sweep points file not generated as sweep point array is empty !");
    }
    QL_DOUT("write_sweep_points() [Done] ");

    return 0;
}

} // namespace sweep_points
} // namespace write
} // namespace io
} // namespace pass
} // namespace ql
