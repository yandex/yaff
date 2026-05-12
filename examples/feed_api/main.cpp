#include <yaff/visitor.h>

#include <iostream>

#include "proto/feed.pb.h"
#include "proto/feed.yaff.h"

feed::FeedResponse BuildFeedResponse() {
    feed::FeedResponse response;
    response.set_request_id(42);

    {
        auto* item = response.add_items();
        item->set_item_id(1001);
        item->set_content_type(feed::CONTENT_TYPE_ARTICLE);
        item->set_title("YaFF: Zero-Copy Serialization for High-Performance Services");
        item->set_description("How YaFF enables direct field access without deserialization overhead.");
        item->set_relevance_score(0.95);
        item->add_tags("yaff");
        item->add_tags("serialization");
        item->add_tags("performance");
        (*item->mutable_extra())["source"] = "engineering-blog";
        (*item->mutable_extra())["read_time"] = "7 min";
        item->set_image_url("https://example.com/img/yaff-overview.png");

        auto* author = item->mutable_author();
        author->set_id(501);
        author->set_name("Alice");
        author->set_verified(true);
    }

    {
        auto* item = response.add_items();
        item->set_item_id(1002);
        item->set_content_type(feed::CONTENT_TYPE_VIDEO);
        item->set_title("Migrating from Protobuf to YaFF: A Practical Guide");
        item->set_relevance_score(0.87);
        item->add_tags("yaff");
        item->add_tags("migration");
        item->set_video_url("https://example.com/v/yaff-migration.mp4");

        auto* author = item->mutable_author();
        author->set_id(502);
        author->set_name("Bob");
    }

    return response;
}

yaff::DetachedBuffer ProtoToYaff(const feed::FeedResponse& proto) {
    yaff::Serializer ys;
    ys.Finish(protoyaff::feed::SerializeFeedResponse(ys, proto));
    return ys.Release();
}

feed::FeedResponse YaffToProto(const yaff::DetachedBuffer& buffer) {
    const auto& yaff = yaff::ReadRoot<protoyaff::feed::FeedResponse>(buffer.Data());
    feed::FeedResponse restored;
    yaff.ParseTo(restored);
    return restored;
}

void PrintHumanReadable(const yaff::DetachedBuffer& buffer) {
    const auto& yaff = yaff::ReadRoot<protoyaff::feed::FeedResponse>(buffer.Data());
    std::cout << yaff::reflect::DebugString(&yaff, protoyaff::feed::FeedResponse::Descriptor()) << "\n";
}

template <typename T>
void Verify(const feed::FeedResponse& original, const T& restored) {
    YAFF_REQUIRE(original.request_id() == restored.request_id());
    YAFF_REQUIRE(original.items_size() == restored.items_size());

    for (int i = 0; i < original.items_size(); ++i) {
        const auto& a = original.items(i);
        const auto& b = restored.items(i);

        YAFF_REQUIRE(a.item_id() == b.item_id());
        YAFF_REQUIRE(static_cast<int>(a.content_type()) == static_cast<int>(b.content_type()));
        YAFF_REQUIRE(a.title() == b.title());
        YAFF_REQUIRE(a.description() == b.description());
        YAFF_REQUIRE(a.relevance_score() == b.relevance_score());
        YAFF_REQUIRE(a.tags_size() == b.tags_size());
        YAFF_REQUIRE(a.extra().size() == b.extra().size());
        YAFF_REQUIRE(a.image_url() == b.image_url());
        YAFF_REQUIRE(a.video_url() == b.video_url());

        if (a.has_author()) {
            YAFF_REQUIRE(b.has_author());
            YAFF_REQUIRE(a.author().id() == b.author().id());
            YAFF_REQUIRE(a.author().name() == b.author().name());
            YAFF_REQUIRE(a.author().verified() == b.author().verified());
        }
    }
}

int main() {
    // 1. Build a protobuf response.
    const auto proto = BuildFeedResponse();

    // 2. Convert to YaFF — a compact, zero-copy readable format.
    const auto buffer = ProtoToYaff(proto);

    // 3. Read YaFF fields directly — no deserialization needed.
    const auto& yaff = yaff::ReadRoot<protoyaff::feed::FeedResponse>(buffer.Data());
    std::cout << "Feed request_id: " << yaff.request_id() << "\n";
    std::cout << "Number of items: " << yaff.items().Size() << "\n";

    const auto& firstItem = yaff.items(0);
    std::cout << "First item title: " << firstItem.title() << "\n";
    std::cout << "First item score: " << firstItem.relevance_score() << "\n\n";

    // 4. In template functions, YaFF can seamlessly replace protobuf.
    Verify(proto, yaff);
    std::cout << "OK: Yaff verification passed.\n";

    // 5. Print YaFF via reflection.
    std::cout << "Human readable representation:\n";
    PrintHumanReadable(buffer);

    // 6. Convert back to protobuf — full round-trip.
    const auto restored = YaffToProto(buffer);
    Verify(proto, restored);
    std::cout << "OK: Round-trip verification passed.\n";

    return 0;
}
