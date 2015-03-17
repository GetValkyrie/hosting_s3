<?php
/**
 * @file
 * Expose the S3 service to Hostmaster.
 */

/**
 * Implementation of hook_hosting_feature().
 */
function hosting_s3_hosting_feature() {
  $features['s3'] = array(
    'title' => t('S3 integration'),
    'description' => t('Allows Aegir to provision S3 buckets for hosted Drupal sites.'),
    'status' => HOSTING_FEATURE_DISABLED,
    'module' => 'hosting_s3',
    'group' => 'experimental'
  );
  return $features;
}
