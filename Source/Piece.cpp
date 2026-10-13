#include "Piece.h"

Piece::Piece (Score& editingScore)
    : editing (editingScore)
{
    pages.push_back (createPage());
    captured.resize (1);
    editing.addChangeListener (this);
}

Piece::~Piece()
{
    editing.removeChangeListener (this);

    for (auto& page : pages)
        page->removeChangeListener (this);
}

std::unique_ptr<Score> Piece::createPage()
{
    auto page = std::make_unique<Score>();
    page->addChangeListener (this);
    return page;
}

Score& Piece::getPage (int page)
{
    jassert (juce::isPositiveAndBelow (page, getNumPages()));
    return page == current ? editing : *pages[(size_t) page];
}

const Score& Piece::getPage (int page) const
{
    jassert (juce::isPositiveAndBelow (page, getNumPages()));
    return page == current ? editing : *pages[(size_t) page];
}

//==============================================================================
void Piece::choosePage (int page)
{
    if (page == current || ! juce::isPositiveAndBelow (page, getNumPages()))
        return;

    {
        // The page left keeps the tempo, which is every page's, along with its music.
        const juce::ScopedValueSetter<bool> quietly (quiet, true);
        pages[(size_t) current]->copyFrom (editing);
        current = page;
        editing.copyFrom (*pages[(size_t) page]);
    }

    // Choosing a page isn't a change to the piece.
    pagesChanged (false);
}

int Piece::addPage()
{
    {
        const juce::ScopedValueSetter<bool> quietly (quiet, true);
        auto page = createPage();
        page->makeBlankPageLike (editing);
        pages.push_back (std::move (page));
        captured.emplace_back();

        // It's the one edited now.
        pages[(size_t) current]->copyFrom (editing);
        current = getNumPages() - 1;
        editing.copyFrom (*pages[(size_t) current]);
    }

    pagesChanged (true);
    return current;
}

int Piece::copyPage (int page)
{
    if (! juce::isPositiveAndBelow (page, getNumPages()))
        return -1;

    const auto copied = page + 1;

    {
        const juce::ScopedValueSetter<bool> quietly (quiet, true);
        auto copy = createPage();
        copy->copyFrom (getPage (page));

        if (const auto& name = copy->getName(); name.isNotEmpty())
            copy->setName (name.substring (0, Score::maxNameLength - 5).trimEnd() + " copy");

        pages.insert (pages.begin() + copied, std::move (copy));
        captured.insert (captured.begin() + copied, std::nullopt);

        // The page being edited moves along if it was after the one copied. The copy's the one edited now.
        if (current >= copied)
            ++current;

        pages[(size_t) current]->copyFrom (editing);
        current = copied;
        editing.copyFrom (*pages[(size_t) current]);
    }

    pagesChanged (true);
    return copied;
}

void Piece::removePage (int page)
{
    if (getNumPages() <= 1 || ! juce::isPositiveAndBelow (page, getNumPages()))
        return;

    {
        const juce::ScopedValueSetter<bool> quietly (quiet, true);

        // Taking out the page being edited, the next one's edited instead, or the one before the last.
        if (page == current)
        {
            current = page + 1 < getNumPages() ? page + 1 : page - 1;
            editing.copyFrom (*pages[(size_t) current]);
        }

        pages[(size_t) page]->removeChangeListener (this);
        pages.erase (pages.begin() + page);
        captured.erase (captured.begin() + page);

        if (current > page)
            --current;
    }

    pagesChanged (true);
}

void Piece::movePage (int from, int to)
{
    if (from == to || ! juce::isPositiveAndBelow (from, getNumPages()) || ! juce::isPositiveAndBelow (to, getNumPages()))
        return;

    // The editing score stays the page it is, which just has another number now.
    auto page = std::move (pages[(size_t) from]);
    pages.erase (pages.begin() + from);
    pages.insert (pages.begin() + to, std::move (page));

    auto text = std::move (captured[(size_t) from]);
    captured.erase (captured.begin() + from);
    captured.insert (captured.begin() + to, std::move (text));

    if (current == from)
        current = to;
    else if (from < current && to >= current)
        --current;
    else if (from > current && to <= current)
        ++current;

    pagesChanged (true);
}

juce::String Piece::getPageName (int page) const
{
    const auto& name = getPage (page).getName();
    return name.isNotEmpty() ? name : "Page " + juce::String (page + 1);
}

void Piece::renamePage (int page, const juce::String& newName)
{
    if (! juce::isPositiveAndBelow (page, getNumPages()))
        return;

    // The name it's shown by without one is no name.
    auto name = Score::tidyName (newName);

    if (name.equalsIgnoreCase ("Page " + juce::String (page + 1)))
        name = {};

    if (name == getPage (page).getName())
        return;

    {
        const juce::ScopedValueSetter<bool> quietly (quiet, true);
        getPage (page).setName (name);
    }

    pagesChanged (true);
}

void Piece::pagesChanged (bool changedPiece)
{
    if (changedPiece)
        sendSynchronousChangeMessage();

    if (onPagesChanged != nullptr)
        onPagesChanged();
}

bool Piece::isPartEmpty (int part) const
{
    for (int page = 0; page < getNumPages(); ++page)
        if (juce::isPositiveAndBelow (part, getPage (page).getNumParts()) && ! getPage (page).isPartEmpty (part))
            return false;

    return true;
}

void Piece::clear()
{
    {
        const juce::ScopedValueSetter<bool> quietly (quiet, true);
        editing.clear();

        for (auto& page : pages)
            page->removeChangeListener (this);

        pages.clear();
        pages.push_back (createPage());
        captured.assign (1, {});
        current = 0;
    }

    pagesChanged (true);
}

//==============================================================================
void Piece::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &editing)
    {
        if (juce::isPositiveAndBelow (current, (int) captured.size()))
            captured[(size_t) current].reset();

        if (quiet)
            return;

        // The other pages keep up with the parts and the tempo.
        matchOtherPages();
        sendSynchronousChangeMessage();
        return;
    }

    // Another page has changed, e.g. by being recorded into while it isn't the one being edited.
    for (size_t page = 0; page < pages.size() && page < captured.size(); ++page)
        if (pages[page].get() == source)
            captured[page].reset();

    if (! quiet)
        sendSynchronousChangeMessage();
}

void Piece::matchOtherPages()
{
    const juce::ScopedValueSetter<bool> quietly (quiet, true);

    for (int page = 0; page < getNumPages(); ++page)
    {
        if (page == current)
            continue;

        auto& other = *pages[(size_t) page];
        other.matchParts (editing);

        if (! juce::exactlyEqual (other.getBeatsPerMinute(), editing.getBeatsPerMinute()))
            other.setBeatsPerMinute (editing.getBeatsPerMinute());
    }
}

//==============================================================================
juce::var Piece::toJSON (bool withPartIds) const
{
    auto json = getPage (0).toJSON (withPartIds);

    if (getNumPages() > 1)
    {
        juce::Array<juce::var> morePages;

        for (int page = 1; page < getNumPages(); ++page)
            morePages.add (getPage (page).toJSON (withPartIds));

        json.getDynamicObject()->setProperty ("morePages", morePages);
    }

    return json;
}

juce::Result Piece::loadJSON (const juce::var& json)
{
    {
        const juce::ScopedValueSetter<bool> quietly (quiet, true);

        // The first page goes into the editing score, whose parts keep their ids, place by place,
        // so their instruments stay with them.
        if (const auto result = editing.loadJSON (json); result.failed())
            return result;

        for (auto& page : pages)
            page->removeChangeListener (this);

        pages.clear();
        pages.push_back (createPage());
        captured.assign (1, {});
        current = 0;

        // The other pages have the first's parts, place by place, and its tempo.
        if (const auto* morePages = json.getProperty ("morePages", {}).getArray())
        {
            for (const auto& pageJSON : *morePages)
            {
                auto page = createPage();

                if (page->loadJSON (pageJSON).failed())
                    continue;

                page->takePartIdsFrom (editing);
                page->matchParts (editing);
                page->setBeatsPerMinute (editing.getBeatsPerMinute());
                pages.push_back (std::move (page));
                captured.emplace_back();
            }
        }
    }

    pagesChanged (true);
    return juce::Result::ok();
}

std::vector<juce::String> Piece::capturePages() const
{
    std::vector<juce::String> texts;

    for (int page = 0; page < getNumPages(); ++page)
    {
        auto& text = captured[(size_t) page];

        if (! text.has_value())
            text = juce::JSON::toString (getPage (page).toJSON (true), true);

        texts.push_back (*text);
    }

    return texts;
}

void Piece::restorePages (const std::vector<juce::String>& texts, int pageToEdit)
{
    if (texts.empty())
        return;

    pageToEdit = juce::jlimit (0, (int) texts.size() - 1, pageToEdit);

    {
        const juce::ScopedValueSetter<bool> quietly (quiet, true);

        for (auto& page : pages)
            page->removeChangeListener (this);

        pages.clear();

        for (size_t page = 0; page < texts.size(); ++page)
        {
            pages.push_back (createPage());

            if ((int) page != pageToEdit)
                pages.back()->loadJSON (juce::JSON::parse (texts[page]));
        }

        current = pageToEdit;
        editing.loadJSON (juce::JSON::parse (texts[(size_t) pageToEdit]));

        // Each page is as captured, so the text's the same for next time.
        captured.assign (texts.begin(), texts.end());
    }

    pagesChanged (true);
}
