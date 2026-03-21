#pragma once

#include "diffsync/core/transfer_log.hpp"
#include "diffsync/types.hpp"
#include <ftxui/component/component.hpp>
#include <string>
#include <functional>
#include <vector>

namespace diffsync {

class LogModal {
public:
    using RetryCallback = std::function<void(const std::vector<ChangeFile>&)>;

    LogModal(TransferLog& transferLog, RetryCallback retryCallback);

    ftxui::Component operator()(bool* externalShown);
    void show();
    void hide();
    void setRetryFiles(const std::vector<ChangeFile>& files);

private:
    TransferLog& transferLog_;
    RetryCallback retryCallback_;
    bool* externalShown_ = nullptr;
    int focusedIndex_ = 0;
    std::vector<std::string> listEntries_;
    std::vector<ChangeFile> retryFiles_;

    void updateListEntries();
    static std::string formatStatus(TransferStatus status);
};

} // namespace diffsync
