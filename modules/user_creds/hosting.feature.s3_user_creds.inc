<?php
/**
 * @file
 * Allow S3 credentials to be inherited from the user account.
 */

/**
 * Implementation of hook_hosting_feature().
 */
function hosting_s3_user_creds_hosting_feature() {
  $features['s3_user_creds'] = array(
    'title' => t('S3 integration - Inherit user credentials'),
    'description' => t('Allow sites to inherit S3 credentials from the user\'s account.'),
    'status' => HOSTING_FEATURE_DISABLED,
    'module' => 'hosting_s3_user_creds',
    'group' => 'experimental'
  );
  return $features;
}
