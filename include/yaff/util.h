#pragma once

#include <iostream>

namespace yaff {

class CodeWriter {
public:
    struct Guard {
        explicit Guard(CodeWriter& p) : Parent(p) {
            Parent.IncrementIdentLevel();
        }

        ~Guard() {
            Parent.DecrementIdentLevel();
        }

        CodeWriter& Parent;
    };

public:
    CodeWriter(std::ostream& underlying, const std::string& ident)
        : Underlying_(underlying), Ident_(ident), IdentLevel_(0), IgnoreIdent_(false), TextWritten_(false) {
    }

    void operator|=(const std::string& text) {
        if (!IgnoreIdent_ && !text.empty()) {
            AppendIdent();
        }

        if (!text.empty() && text.back() == '\\') {
            IgnoreIdent_ = true;
            Underlying_ << text.substr(0, text.size() - 1);
        } else {
            IgnoreIdent_ = false;
            Underlying_ << text << '\n';
        }
        TextWritten_ = true;
    }

    void operator>=(const std::string& text) {
        auto guard = IdentGuard();
        this->operator|=(text);
    }

    void IncrementIdentLevel() {
        ++IdentLevel_;
    }

    void DecrementIdentLevel() {
        if (IdentLevel_ > 0) {
            --IdentLevel_;
        }
    }

    void TrackText() {
        TextWritten_ = false;
    }

    bool TextWritten() {
        return TextWritten_;
    }

    Guard IdentGuard() {
        return Guard{*this};
    }

private:
    void AppendIdent() {
        for (int i = 0; i < IdentLevel_; ++i) {
            Underlying_.write(Ident_.data(), Ident_.size());
        }
    }

    std::ostream& Underlying_;
    std::string Ident_;

    int IdentLevel_;
    bool IgnoreIdent_;
    bool TextWritten_;
};

}  // namespace yaff
