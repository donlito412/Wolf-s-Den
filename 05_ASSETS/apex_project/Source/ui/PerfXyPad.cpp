#include "PerfXyPad.h"

#include "../PluginProcessor.h"

namespace wolfsden
{
namespace
{
constexpr int kPadSize = 400;

inline juce::Point<float> normFromLocal(juce::Rectangle<float> bounds, juce::Point<float> local) noexcept
{
    const float x = (local.x - (float)bounds.getX()) / juce::jmax(1.f, (float)bounds.getWidth());
    const float y = (local.y - (float)bounds.getY()) / juce::jmax(1.f, (float)bounds.getHeight());
    return { juce::jlimit(0.f, 1.f, x), juce::jlimit(0.f, 1.f, y) };
}
} // namespace

PerfXyPad::PerfXyPad(WolfsDenAudioProcessor& p)
    : proc(p)
{
    setSize(kPadSize, kPadSize);
    const float ix = proc.getModMatrix().getXyX();
    const float iy = proc.getModMatrix().getXyY();
    targetX = posX = ix;
    targetY = posY = iy;
    startTimerHz(60);
}

PerfXyPad::~PerfXyPad()
{
    stopTimer();
}

int PerfXyPad::getPhysicsMode() const noexcept
{
    auto& ap = proc.getAPVTS();
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*>(ap.getParameter("perf_xy_physics")))
        return c->getIndex();
    return 0;
}

void PerfXyPad::updateFromMouse(const juce::MouseEvent& e)
{
    const auto n = normFromLocal(getLocalBounds().toFloat(), e.position);
    targetX = n.x;
    targetY = n.y;
}

void PerfXyPad::pushPosition()
{
    proc.getModMatrix().setXy(posX, posY);
}

void PerfXyPad::timerCallback()
{
    const int mode = getPhysicsMode();

    if (mode == 0)
    {
        if (dragging)
        {
            posX = targetX;
            posY = targetY;
        }
    }
    else if (mode == 1)
    {
        constexpr float attraction = 0.2f;
        constexpr float damping = 0.9f;
        velX = (velX + (targetX - posX) * attraction) * damping;
        velY = (velY + (targetY - posY) * attraction) * damping;
        posX += velX;
        posY += velY;
        posX = juce::jlimit(0.f, 1.f, posX);
        posY = juce::jlimit(0.f, 1.f, posY);
    }
    else
    {
        constexpr double sigma = 10.0;
        constexpr double rho = 28.0;
        constexpr double beta = 8.0 / 3.0;
        constexpr double dt = 0.005;

        const double dx = sigma * (lorenzY - lorenzX);
        const double dy = lorenzX * (rho - lorenzZ) - lorenzY;
        const double dz = lorenzX * lorenzY - beta * lorenzZ;
        lorenzX += dx * dt;
        lorenzY += dy * dt;
        lorenzZ += dz * dt;

        float nx = juce::jlimit(0.f, 1.f, 0.5f + 0.028f * (float)lorenzX);
        float ny = juce::jlimit(0.f, 1.f, 0.5f + 0.028f * (float)lorenzY);
        constexpr float bias = 0.04f;
        posX = nx * (1.f - bias) + targetX * bias;
        posY = ny * (1.f - bias) + targetY * bias;
        posX = juce::jlimit(0.f, 1.f, posX);
        posY = juce::jlimit(0.f, 1.f, posY);
    }

    pushPosition();
    if (onPositionChanged)
        onPositionChanged(posX, posY);
    repaint();
}

void PerfXyPad::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(r, 6.f);
    g.setColour(juce::Colour(0xff555555));
    g.drawRoundedRectangle(r.reduced(0.5f), 6.f, 1.f);

    const float cx = r.getX() + posX * r.getWidth();
    const float cy = r.getY() + posY * r.getHeight();
    g.setColour(juce::Colour(0xff88ccee));
    g.drawLine(r.getX(), cy, r.getRight(), cy, 1.f);
    g.drawLine(cx, r.getY(), cx, r.getBottom(), 1.f);
    g.setColour(juce::Colours::white);
    g.fillEllipse(cx - 6.f, cy - 6.f, 12.f, 12.f);
}

void PerfXyPad::resized()
{
    juce::ignoreUnused(getWidth(), getHeight());
}

void PerfXyPad::mouseDown(const juce::MouseEvent& e)
{
    dragging = true;
    updateFromMouse(e);
    if (getPhysicsMode() == 0)
    {
        posX = targetX;
        posY = targetY;
        pushPosition();
    }
    repaint();
}

void PerfXyPad::mouseDrag(const juce::MouseEvent& e)
{
    updateFromMouse(e);
    if (getPhysicsMode() == 0)
    {
        posX = targetX;
        posY = targetY;
        pushPosition();
    }
    repaint();
}

void PerfXyPad::mouseUp(const juce::MouseEvent& e)
{
    juce::ignoreUnused(e);
    dragging = false;
}

} // namespace wolfsden
