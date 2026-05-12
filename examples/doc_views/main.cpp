#include <yaff/visitor.h>

#include <iostream>

#include "proto/document.pb.h"
#include "proto/document_full_view.yaff.h"
#include "proto/document_light_view.yaff.h"

document::Document LoadDocumentFromStorage() {
    document::Document doc;
    doc.set_doc_id(100500);

    // Light fields.
    doc.set_category(document::CATEGORY_NEWS);
    doc.set_bid_micros(1500000);
    doc.set_region_id(213);
    doc.set_is_active(true);

    // Heavy fields.
    doc.set_title("YaFF: Efficient Serialization for Ranking Pipelines");
    doc.add_embedding(0.12);
    doc.add_embedding(0.87);
    doc.add_embedding(-0.34);
    doc.add_embedding(0.56);
    doc.add_keywords("serialization");
    doc.add_keywords("ranking");
    doc.add_keywords("performance");

    auto* quality = doc.mutable_quality();
    quality->set_relevance(0.92);
    quality->set_freshness(0.78);
    quality->set_user_engagement(0.65);

    return doc;
}

yaff::DetachedBuffer BuildLightView(const document::Document& proto) {
    yaff::Serializer ys;
    ys.Finish(NLightView::document::SerializeDocument(ys, proto));
    return ys.Release();
}

yaff::DetachedBuffer BuildFullView(const document::Document& proto) {
    yaff::Serializer ys;
    ys.Finish(NFullView::document::SerializeDocument(ys, proto));
    return ys.Release();
}

void DemoLightView(const yaff::DetachedBuffer& buffer) {
    const auto& view = yaff::ReadRoot<NLightView::document::Document>(buffer.Data());

    std::cout << "=== Light View (early-stage filtering) ===\n";
    std::cout << "Size: " << buffer.Size() << " bytes\n";
    std::cout << "doc_id:    " << view.doc_id() << "\n";
    std::cout << "category:  " << static_cast<int>(view.category()) << "\n";
    std::cout << "bid:       " << view.bid_micros() << " micros\n";
    std::cout << "region_id: " << view.region_id() << "\n";
    std::cout << "is_active: " << view.is_active() << "\n";
    std::cout << "\nHuman Readable:\n";
    std::cout << yaff::reflect::DebugString(&view, NLightView::document::Document::Descriptor()) << "\n\n";
}

void DemoFullView(const yaff::DetachedBuffer& buffer) {
    const auto& view = yaff::ReadRoot<NFullView::document::Document>(buffer.Data());

    std::cout << "=== Full View (late-stage ranking) ===\n";
    std::cout << "Size: " << buffer.Size() << " bytes\n";
    std::cout << "doc_id:    " << view.doc_id() << "\n";
    std::cout << "title:     " << view.title() << "\n";

    std::cout << "embedding: [";
    const auto& emb = view.embedding();
    for (uint32_t i = 0; i < emb.Size(); ++i) {
        if (i > 0) {
            std::cout << ", ";
        }
        std::cout << emb.Get(i);
    }
    std::cout << "]\n";

    std::cout << "quality:   relevance=" << view.quality().relevance() << " freshness=" << view.quality().freshness()
              << " engagement=" << view.quality().user_engagement() << "\n";

    std::cout << "keywords:  [";
    const auto& kw = view.keywords();
    for (uint32_t i = 0; i < kw.Size(); ++i) {
        if (i > 0) {
            std::cout << ", ";
        }
        std::cout << kw.Get(i);
    }
    std::cout << "]\n";

    std::cout << "\nHuman Readable:\n";
    std::cout << yaff::reflect::DebugString(&view, NFullView::document::Document::Descriptor()) << "\n\n";
}

void VerifyRoundTrip(const document::Document& original, const yaff::DetachedBuffer& lightBuf,
                     const yaff::DetachedBuffer& fullBuf) {
    // Verify light view round-trip.
    {
        const auto& view = yaff::ReadRoot<NLightView::document::Document>(lightBuf.Data());
        document::Document restored;
        view.ParseTo(restored);

        YAFF_REQUIRE(original.doc_id() == restored.doc_id());
        YAFF_REQUIRE(original.category() == restored.category());
        YAFF_REQUIRE(original.bid_micros() == restored.bid_micros());
        YAFF_REQUIRE(original.region_id() == restored.region_id());
        YAFF_REQUIRE(original.is_active() == restored.is_active());
        // Full-view fields should NOT be present in the light view restoration.
        YAFF_REQUIRE(restored.title().empty());
        YAFF_REQUIRE(restored.embedding_size() == 0);
    }

    // Verify full view round-trip.
    {
        const auto& view = yaff::ReadRoot<NFullView::document::Document>(fullBuf.Data());
        document::Document restored;
        view.ParseTo(restored);

        YAFF_REQUIRE(original.doc_id() == restored.doc_id());
        YAFF_REQUIRE(original.title() == restored.title());
        YAFF_REQUIRE(original.embedding_size() == restored.embedding_size());
        YAFF_REQUIRE(original.quality().relevance() == restored.quality().relevance());
        YAFF_REQUIRE(original.keywords_size() == restored.keywords_size());
        // Light-view-only fields should NOT be present in the full view restoration.
        YAFF_REQUIRE(restored.bid_micros() == 0);
        YAFF_REQUIRE(restored.region_id() == 0);
    }

    std::cout << "Round-trip verification passed for both views.\n";
}

int main() {
    // 1. Load a document from storage (full protobuf).
    const auto proto = LoadDocumentFromStorage();

    // 2. Build different YaFF views from the same protobuf.
    const auto lightBuf = BuildLightView(proto);
    const auto fullBuf = BuildFullView(proto);

    // 3. Demonstrate each view.
    DemoLightView(lightBuf);
    DemoFullView(fullBuf);

    // 4. Verify round-trip for both views.
    VerifyRoundTrip(proto, lightBuf, fullBuf);

    return 0;
}
