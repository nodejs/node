async function getCookieNames() {
  const cookies = await test_driver.get_all_cookies();
  const cookie_names = {};
  cookies.forEach((element) => cookie_names[element["name"]] = true);
  let message = ",SetOnRedirectLoad:";
  if (cookie_names.hasOwnProperty("samesite_strict_set_on_redirect_load")) {
    message += "Strict";
  }
  if (cookie_names.hasOwnProperty("samesite_lax_set_on_redirect_load")) {
      message += "Lax";
  }
  if (cookie_names.hasOwnProperty("samesite_none_set_on_redirect_load")) {
      message += "None";
  }
  message += ",SetOnLoad:";
  if (cookie_names.hasOwnProperty("samesite_strict_set_on_load")) {
    message += "Strict";
  }
  if (cookie_names.hasOwnProperty("samesite_lax_set_on_load")) {
      message += "Lax";
  }
  if (cookie_names.hasOwnProperty("samesite_none_set_on_load")) {
      message += "None";
  }
  message += ",SetOnRedirectFetch:";
  if (cookie_names.hasOwnProperty("samesite_strict_set_on_redirect_fetch")) {
    message += "Strict";
  }
  if (cookie_names.hasOwnProperty("samesite_lax_set_on_redirect_fetch")) {
      message += "Lax";
  }
  if (cookie_names.hasOwnProperty("samesite_none_set_on_redirect_fetch")) {
      message += "None";
  }
  message += ",SetOnFetch:";
  if (cookie_names.hasOwnProperty("samesite_strict_set_on_fetch")) {
    message += "Strict";
  }
  if (cookie_names.hasOwnProperty("samesite_lax_set_on_fetch")) {
      message += "Lax";
  }
  if (cookie_names.hasOwnProperty("samesite_none_set_on_fetch")) {
      message += "None";
  }
  return message;
}
