# UI Regression Smoke Checklist

Use this checklist after visual updates to validate the shared design system remains consistent.

## Global foundation
- [ ] `assets/css/design-system.css` loads on public and authenticated pages.
- [ ] Heading/body typography scale is consistent and readable.
- [ ] Card radius, border treatment, and elevation feel consistent across surfaces.
- [ ] Focus ring appears for keyboard navigation on links, buttons, and form controls.

## Interaction polish
- [ ] Primary CTA buttons (donation, upgrade, action buttons) share hover/focus/active motion.
- [ ] Navigation links (top nav, command nav, vault nav, back links) share the same transition cadence.
- [ ] Interactive pills/chips keep readable contrast in idle/hover/active states.

## Page-by-page checks

### Public home (`index.php`)
- [ ] Hero navigation links and quick-jump pills animate consistently.
- [ ] Donation CTA and recommendation links use standardized transitions/focus.

### Dashboard (`dashboard.php`)
- [ ] Command topbar links and Resource+ CTA animate and focus consistently.
- [ ] Cards maintain shared radius/elevation language.

### Admin (`admin.php`)
- [ ] Sidebar command navigation focus/hover states align with global link polish.
- [ ] Panel spacing and card hierarchy remain legible on desktop and mobile.

### Commlink (`commlink.php`)
- [ ] Back link, ghost buttons, and action buttons share interaction timing.
- [ ] Focus treatment is clearly visible in chat, mail, and forum controls.

### Resources (`resources.php`)
- [ ] Vault nav and membership CTA interactions match global polish.
- [ ] Briefing cards and hero sections retain visual rhythm with shared spacing/shadow tokens.
