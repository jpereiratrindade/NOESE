// Shared browser checks for navigation, local assets and responsive reading space.
const assert = require("node:assert/strict");

module.exports = async function checkIdentity(page) {
  const previous = page.viewportSize();
  const current = new URL(page.url()).pathname;
  assert.equal(
    await page
      .locator('.sister-nav [aria-current="page"]')
      .getAttribute("href"),
    current,
  );
  for (const asset of ["/noese-symbol.svg", "/favicon.svg"]) {
    const response = await page.request.get(new URL(asset, page.url()).href);
    assert.equal(response.status(), 200);
    assert.match(response.headers()["content-type"], /^image\/svg\+xml/);
  }
  for (const width of [320, 390, 768, 860, 861, 1024, 1440]) {
    await page.setViewportSize({ width, height: 1000 });
    assert.ok(
      await page.evaluate(
        () => document.documentElement.scrollWidth <= innerWidth,
      ),
      `Horizontal overflow on ${current} at ${width}px`,
    );
  }
  await page.setViewportSize({ width: 390, height: 844 });
  const menu = page.locator("#sister-menu");
  assert.equal(await page.locator("#sister-sidebar").isVisible(), false);
  await menu.click();
  assert.equal(await menu.getAttribute("aria-expanded"), "true");
  assert.equal(
    await page.locator(".sister-workspace").evaluate((e) => e.inert),
    true,
  );
  assert.equal(
    await page
      .locator("#sister-nav-close")
      .evaluate((e) => e === document.activeElement),
    true,
  );
  await page.keyboard.press("Shift+Tab");
  assert.equal(
    await page
      .locator(".sister-nav a:last-child")
      .evaluate((e) => e === document.activeElement),
    true,
  );
  await page.keyboard.press("Tab");
  assert.equal(
    await page
      .locator("#sister-nav-close")
      .evaluate((e) => e === document.activeElement),
    true,
  );
  await page.keyboard.press("Escape");
  assert.equal(await menu.getAttribute("aria-expanded"), "false");
  assert.equal(await menu.evaluate((e) => e === document.activeElement), true);
  assert.equal(
    await page.locator(".sister-workspace").evaluate((e) => e.inert),
    false,
  );
  await menu.click();
  await page.locator("#sister-scrim").click({ position: { x: 380, y: 150 } });
  assert.equal(await menu.getAttribute("aria-expanded"), "false");
  await page.setViewportSize(previous);
};
