/*
 * Unit tests for crypt functions
 *
 * Copyright (c) 2004 Michael Jung
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "wincrypt.h"
#include "winerror.h"
#include "winreg.h"

#include "wine/test.h"

static const char szRsaBaseProv[] = MS_DEF_PROV_A;
static const char szNonExistentProv[] = "Wine Non Existent Cryptographic Provider v11.2";
static const char szKeySet[] = "wine_test_keyset";
static const char szBadKeySet[] = "wine_test_bad_keyset";
#define NON_DEF_PROV_TYPE 999

static void init_environment(void)
{
	HCRYPTPROV hProv;
	
	/* Ensure that container "wine_test_keyset" does exist */
	if (!CryptAcquireContext(&hProv, szKeySet, szRsaBaseProv, PROV_RSA_FULL, 0))
	{
		CryptAcquireContext(&hProv, szKeySet, szRsaBaseProv, PROV_RSA_FULL, CRYPT_NEWKEYSET);
	}
	CryptReleaseContext(hProv, 0);

	/* Ensure that container "wine_test_keyset" does exist in default PROV_RSA_FULL type provider */
	if (!CryptAcquireContext(&hProv, szKeySet, NULL, PROV_RSA_FULL, 0))
	{
		CryptAcquireContext(&hProv, szKeySet, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET);
	}
	CryptReleaseContext(hProv, 0);

	/* Ensure that container "wine_test_bad_keyset" does not exist. */
	if (CryptAcquireContext(&hProv, szBadKeySet, szRsaBaseProv, PROV_RSA_FULL, 0))
	{
		CryptReleaseContext(hProv, 0);
		CryptAcquireContext(&hProv, szBadKeySet, szRsaBaseProv, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
	}
}

static void clean_up_environment(void)
{
	HCRYPTPROV hProv;

	/* Remove container "wine_test_keyset" */
	if (CryptAcquireContext(&hProv, szKeySet, szRsaBaseProv, PROV_RSA_FULL, 0))
	{
		CryptReleaseContext(hProv, 0);
		CryptAcquireContext(&hProv, szKeySet, szRsaBaseProv, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
	}

	/* Remove container "wine_test_keyset" from default PROV_RSA_FULL type provider */
	if (CryptAcquireContext(&hProv, szKeySet, NULL, PROV_RSA_FULL, 0))
	{
		CryptReleaseContext(hProv, 0);
		CryptAcquireContext(&hProv, szKeySet, NULL, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
	}
}

static void test_acquire_context(void)
{
	BOOL result;
	HCRYPTPROV hProv;

	/* Provoke all kinds of error conditions (which are easy to provoke). 
	 * The order of the error tests seems to match Windows XP's rsaenh.dll CSP,
	 * but since this is likely to change between CSP versions, we don't check
	 * this. Please don't change the order of tests. */
	result = CryptAcquireContext(&hProv, NULL, NULL, 0, 0);
	ok(!result && GetLastError()==NTE_BAD_PROV_TYPE, "%ld\n", GetLastError());
	
	result = CryptAcquireContext(&hProv, NULL, NULL, 1000, 0);
	ok(!result && GetLastError()==NTE_BAD_PROV_TYPE, "%ld\n", GetLastError());

	result = CryptAcquireContext(&hProv, NULL, NULL, NON_DEF_PROV_TYPE, 0);
	ok(!result && GetLastError()==NTE_PROV_TYPE_NOT_DEF, "%ld\n", GetLastError());
	
	result = CryptAcquireContext(&hProv, szKeySet, szNonExistentProv, PROV_RSA_FULL, 0);
	ok(!result && GetLastError()==NTE_KEYSET_NOT_DEF, "%ld\n", GetLastError());

	result = CryptAcquireContext(&hProv, szKeySet, szRsaBaseProv, NON_DEF_PROV_TYPE, 0);
	ok(!result && GetLastError()==NTE_PROV_TYPE_NO_MATCH, "%ld\n", GetLastError());
	
	/* This test fails under Win2k SP4:
	   result = TRUE, GetLastError() == ERROR_INVALID_PARAMETER
	SetLastError(0xdeadbeef);
	result = CryptAcquireContext(NULL, szKeySet, szRsaBaseProv, PROV_RSA_FULL, 0);
	ok(!result && GetLastError()==ERROR_INVALID_PARAMETER, "%d/%ld\n", result, GetLastError());
	*/
	
	/* Last not least, try to really acquire a context. */
	hProv = 0;
	SetLastError(0xdeadbeef);
	result = CryptAcquireContext(&hProv, szKeySet, szRsaBaseProv, PROV_RSA_FULL, 0);
	ok(result && GetLastError() == ERROR_SUCCESS, "%d/%ld\n", result, GetLastError());

	if (hProv) 
		CryptReleaseContext(hProv, 0);

	/* Try again, witch an empty ("\0") szProvider parameter */
	hProv = 0;
	SetLastError(0xdeadbeef);
	result = CryptAcquireContext(&hProv, szKeySet, "", PROV_RSA_FULL, 0);
	ok(result && GetLastError() == ERROR_SUCCESS, "%d/%ld\n", result, GetLastError());

	if (hProv) 
		CryptReleaseContext(hProv, 0);
}

static BOOL FindProvRegVals(DWORD dwIndex, DWORD *pdwProvType, LPSTR *pszProvName, 
			    DWORD *pcbProvName, DWORD *pdwProvCount)
{
	HKEY hKey;
	HKEY subkey;
	DWORD size = sizeof(DWORD);
	
	if (RegOpenKey(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Cryptography\\Defaults\\Provider", &hKey))
		return FALSE;
	
	RegQueryInfoKey(hKey, NULL, NULL, NULL, pdwProvCount, pcbProvName, 
				 NULL, NULL, NULL, NULL, NULL, NULL);
	(*pcbProvName)++;
	
	if (!(*pszProvName = ((LPSTR)LocalAlloc(LMEM_ZEROINIT, *pcbProvName))))
		return FALSE;
	
	RegEnumKeyEx(hKey, dwIndex, *pszProvName, pcbProvName, NULL, NULL, NULL, NULL);
	(*pcbProvName)++;

	RegOpenKey(hKey, *pszProvName, &subkey);
	RegQueryValueEx(subkey, "Type", NULL, NULL, (BYTE*)pdwProvType, &size);
	
	RegCloseKey(subkey);
	RegCloseKey(hKey);
	
	return TRUE;
}

static void test_enum_providers(void)
{
	/* expected results */
	CHAR *pszProvName = NULL;
	DWORD cbName;
	DWORD dwType;
	DWORD provCount;
	DWORD dwIndex = 0;
	
	/* actual results */
	CHAR *provider = NULL;
	DWORD providerLen;
	DWORD type;
	DWORD count;
	BOOL result;
	DWORD notNull = 5;
	DWORD notZeroFlags = 5;
	
	if (!FindProvRegVals(dwIndex, &dwType, &pszProvName, &cbName, &provCount))
		return;
	
	/* check pdwReserved flag for NULL */
	result = CryptEnumProviders(dwIndex, &notNull, 0, &type, NULL, &providerLen);
	ok(!result && GetLastError()==ERROR_INVALID_PARAMETER, "%ld\n", GetLastError());
	
	/* check dwFlags == 0 */
	result = CryptEnumProviders(dwIndex, NULL, notZeroFlags, &type, NULL, &providerLen);
	ok(!result && GetLastError()==NTE_BAD_FLAGS, "%ld\n", GetLastError());
	
	/* alloc provider to half the size required
	 * cbName holds the size required */
	providerLen = cbName / 2;
	if (!(provider = ((LPSTR)LocalAlloc(LMEM_ZEROINIT, providerLen))))
		return;

	result = CryptEnumProviders(dwIndex, NULL, 0, &type, provider, &providerLen);
	ok(!result && GetLastError()==ERROR_MORE_DATA, "expected %i, got %ld\n",
		ERROR_MORE_DATA, GetLastError());

	LocalFree(provider);

	/* loop through the providers to get the number of providers 
	 * after loop ends, count should be provCount + 1 so subtract 1
	 * to get actual number of providers */
	count = 0;
	while(CryptEnumProviders(count++, NULL, 0, &type, NULL, &providerLen))
		;
	count--;
	ok(count==provCount, "expected %i, got %i\n", (int)provCount, (int)count);
	
	/* loop past the actual number of providers to get the error
	 * ERROR_NO_MORE_ITEMS */
	for (count = 0; count < provCount + 1; count++)
		result = CryptEnumProviders(count, NULL, 0, &type, NULL, &providerLen);
	ok(!result && GetLastError()==ERROR_NO_MORE_ITEMS, "expected %i, got %ld\n", 
			ERROR_NO_MORE_ITEMS, GetLastError());
	
	/* check expected versus actual values returned */
	result = CryptEnumProviders(dwIndex, NULL, 0, &type, NULL, &providerLen);
	ok(result && providerLen==cbName, "expected %i, got %i\n", (int)cbName, (int)providerLen);
	if (!(provider = ((LPSTR)LocalAlloc(LMEM_ZEROINIT, providerLen))))
		return;
		
	result = CryptEnumProviders(dwIndex, NULL, 0, &type, provider, &providerLen);
	ok(result && type==dwType, "expected %ld, got %ld\n", 
		dwType, type);
	ok(result && !strcmp(pszProvName, provider), "expected %s, got %s\n", pszProvName, provider);
	ok(result && cbName==providerLen, "expected %ld, got %ld\n", 
		cbName, providerLen);

	LocalFree(provider);
}

static BOOL FindProvTypesRegVals(DWORD dwIndex, DWORD *pdwProvType, LPSTR *pszTypeName, 
				 DWORD *pcbTypeName, DWORD *pdwTypeCount)
{
	HKEY hKey;
	HKEY hSubKey;
	PSTR ch;
	
	if (RegOpenKey(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Cryptography\\Defaults\\Provider Types", &hKey))
		return FALSE;
	
	if (RegQueryInfoKey(hKey, NULL, NULL, NULL, pdwTypeCount, pcbTypeName, NULL,
			NULL, NULL, NULL, NULL, NULL))
	    return FALSE;
	(*pcbTypeName)++;
	
	if (!(*pszTypeName = ((LPSTR)LocalAlloc(LMEM_ZEROINIT, *pcbTypeName))))
		return FALSE;
	
	if (RegEnumKeyEx(hKey, dwIndex, *pszTypeName, pcbTypeName, NULL, NULL, NULL, NULL))
	    return FALSE;
	(*pcbTypeName)++;
	ch = *pszTypeName + strlen(*pszTypeName);
	/* Convert "Type 000" to 0, etc/ */
	*pdwProvType = *(--ch) - '0';
	*pdwProvType += (*(--ch) - '0') * 10;
	*pdwProvType += (*(--ch) - '0') * 100;
	
	if (RegOpenKey(hKey, *pszTypeName, &hSubKey))
	    return FALSE;
	
	if (RegQueryValueEx(hSubKey, "TypeName", NULL, NULL, NULL, pcbTypeName))
            return FALSE;

	if (!(*pszTypeName = ((LPSTR)LocalAlloc(LMEM_ZEROINIT, *pcbTypeName))))
		return FALSE;
	
	if (RegQueryValueEx(hSubKey, "TypeName", NULL, NULL, *pszTypeName, pcbTypeName))
	    return FALSE;
	
	RegCloseKey(hSubKey);
	RegCloseKey(hKey);
	
	return TRUE;
}

static void test_enum_provider_types()
{
	/* expected values */
	DWORD dwProvType;
	LPSTR pszTypeName = NULL;
	DWORD cbTypeName;
	DWORD dwTypeCount;
	
	/* actual values */
	DWORD index = 0;
	DWORD provType;
	LPSTR typeName = NULL;
	DWORD typeNameSize;
	DWORD typeCount;
	DWORD result;
	DWORD notNull = 5;
	DWORD notZeroFlags = 5;
	
	if (!FindProvTypesRegVals(index, &dwProvType, &pszTypeName, &cbTypeName, &dwTypeCount))
	{
	    trace("could not find provider types in registry, skipping the test\n");
	    return;
	}
	
	/* check pdwReserved for NULL */
	result = CryptEnumProviderTypes(index, &notNull, 0, &provType, typeName, &typeNameSize);
	ok(!result && GetLastError()==ERROR_INVALID_PARAMETER, "expected %i, got %ld\n", 
		ERROR_INVALID_PARAMETER, GetLastError());
	
	/* check dwFlags == zero */
	result = CryptEnumProviderTypes(index, NULL, notZeroFlags, &provType, typeName, &typeNameSize);
	ok(!result && GetLastError()==NTE_BAD_FLAGS, "expected %i, got %ld\n",
		ERROR_INVALID_PARAMETER, GetLastError());
	
	/* alloc provider type to half the size required
	 * cbTypeName holds the size required */
	typeNameSize = cbTypeName / 2;
	if (!(typeName = ((LPSTR)LocalAlloc(LMEM_ZEROINIT, typeNameSize))))
		return;

	/* This test fails under Win2k SP4:
	   result = TRUE, GetLastError() == 0xdeadbeef
	SetLastError(0xdeadbeef);
	result = CryptEnumProviderTypes(index, NULL, 0, &provType, typeName, &typeNameSize);
	ok(!result && GetLastError()==ERROR_MORE_DATA, "expected 0/ERROR_MORE_DATA, got %d/%08lx\n",
		result, GetLastError());
	*/
	
	LocalFree(typeName);
	
	/* loop through the provider types to get the number of provider types 
	 * after loop ends, count should be dwTypeCount + 1 so subtract 1
	 * to get actual number of provider types */
	typeCount = 0;
	while(CryptEnumProviderTypes(typeCount++, NULL, 0, &provType, NULL, &typeNameSize))
		;
	typeCount--;
	ok(typeCount==dwTypeCount, "expected %ld, got %ld\n", dwTypeCount, typeCount);
	
	/* loop past the actual number of provider types to get the error
	 * ERROR_NO_MORE_ITEMS */
	for (typeCount = 0; typeCount < dwTypeCount + 1; typeCount++)
		result = CryptEnumProviderTypes(typeCount, NULL, 0, &provType, NULL, &typeNameSize);
	ok(!result && GetLastError()==ERROR_NO_MORE_ITEMS, "expected %i, got %ld\n", 
			ERROR_NO_MORE_ITEMS, GetLastError());
	

	/* check expected versus actual values returned */
	result = CryptEnumProviderTypes(index, NULL, 0, &provType, NULL, &typeNameSize);
	ok(result && typeNameSize==cbTypeName, "expected %ld, got %ld\n", cbTypeName, typeNameSize);
	if (!(typeName = ((LPSTR)LocalAlloc(LMEM_ZEROINIT, typeNameSize))))
		return;
		
	typeNameSize = 0xdeadbeef;
	result = CryptEnumProviderTypes(index, NULL, 0, &provType, typeName, &typeNameSize);
	ok(result, "expected TRUE, got %ld\n", result);
	ok(provType==dwProvType, "expected %ld, got %ld\n", dwProvType, provType);
	if (pszTypeName)
	    ok(!strcmp(pszTypeName, typeName), "expected %s, got %s\n", pszTypeName, typeName);
	ok(typeNameSize==cbTypeName, "expected %ld, got %ld\n", cbTypeName, typeNameSize);
	
	LocalFree(typeName);
}

static BOOL FindDfltProvRegVals(DWORD dwProvType, DWORD dwFlags, LPSTR *pszProvName, DWORD *pcbProvName)
{
	HKEY hKey;
	PSTR keyname;
	PSTR ptr;
	DWORD user = dwFlags & CRYPT_USER_DEFAULT;
	
	LPSTR MACHINESTR = "Software\\Microsoft\\Cryptography\\Defaults\\Provider Types\\Type XXX";
	LPSTR USERSTR = "Software\\Microsoft\\Cryptography\\Provider Type XXX";
	
	keyname = LocalAlloc(LMEM_ZEROINIT, (user ? strlen(USERSTR) : strlen(MACHINESTR)) + 1);
	if (keyname)
	{
		user ? strcpy(keyname, USERSTR) : strcpy(keyname, MACHINESTR);
		ptr = keyname + strlen(keyname);
		*(--ptr) = (dwProvType % 10) + '0';
		*(--ptr) = ((dwProvType / 10) % 10) + '0';
		*(--ptr) = (dwProvType / 100) + '0';
	} else
		return FALSE;
	
	if (RegOpenKey((dwFlags & CRYPT_USER_DEFAULT) ?  HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE ,keyname, &hKey))
	{
		LocalFree(keyname);
		return FALSE;
	}
	LocalFree(keyname);
	
	if (RegQueryValueEx(hKey, "Name", NULL, NULL, *pszProvName, pcbProvName))
	{
		if (GetLastError() != ERROR_MORE_DATA)
			SetLastError(NTE_PROV_TYPE_ENTRY_BAD);
		return FALSE;
	}
	
	if (!(*pszProvName = LocalAlloc(LMEM_ZEROINIT, *pcbProvName)))
		return FALSE;
	
	if (RegQueryValueEx(hKey, "Name", NULL, NULL, *pszProvName, pcbProvName))
	{
		if (GetLastError() != ERROR_MORE_DATA)
			SetLastError(NTE_PROV_TYPE_ENTRY_BAD);
		return FALSE;
	}
	
	RegCloseKey(hKey);
	
	return TRUE;
}

static void test_get_default_provider()
{
	/* expected results */
	DWORD dwProvType = PROV_RSA_FULL;
	DWORD dwFlags = CRYPT_MACHINE_DEFAULT;
	LPSTR pszProvName = NULL;
	DWORD cbProvName;
	
	/* actual results */
	DWORD provType = PROV_RSA_FULL;
	DWORD flags = CRYPT_MACHINE_DEFAULT;
	LPSTR provName = NULL;
	DWORD provNameSize;
	DWORD result;
	DWORD notNull = 5;
	
	FindDfltProvRegVals(dwProvType, dwFlags, &pszProvName, &cbProvName);
	
	/* check pdwReserved for NULL */
	result = CryptGetDefaultProvider(provType, &notNull, flags, provName, &provNameSize);
	ok(!result && GetLastError()==ERROR_INVALID_PARAMETER, "expected %i, got %ld\n",
		ERROR_INVALID_PARAMETER, GetLastError());
	
	/* check for invalid flag */
	flags = 0xdeadbeef;
	result = CryptGetDefaultProvider(provType, NULL, flags, provName, &provNameSize);
	ok(!result && GetLastError()==NTE_BAD_FLAGS, "expected %ld, got %ld\n",
		NTE_BAD_FLAGS, GetLastError());
	flags = CRYPT_MACHINE_DEFAULT;
	
	/* check for invalid prov type */
	provType = 0xdeadbeef;
	result = CryptGetDefaultProvider(provType, NULL, flags, provName, &provNameSize);
	ok(!result && (GetLastError() == NTE_BAD_PROV_TYPE ||
	               GetLastError() == ERROR_INVALID_PARAMETER),
		"expected NTE_BAD_PROV_TYPE or ERROR_INVALID_PARAMETER, got %ld/%ld\n",
		result, GetLastError());
	provType = PROV_RSA_FULL;
	
	SetLastError(0);
	
	/* alloc provName to half the size required
	 * cbProvName holds the size required */
	provNameSize = cbProvName / 2;
	if (!(provName = LocalAlloc(LMEM_ZEROINIT, provNameSize)))
		return;
	
	result = CryptGetDefaultProvider(provType, NULL, flags, provName, &provNameSize);
	ok(!result && GetLastError()==ERROR_MORE_DATA, "expected %i, got %ld\n",
		ERROR_MORE_DATA, GetLastError());
		
	LocalFree(provName);
	
	/* check expected versus actual values returned */
	result = CryptGetDefaultProvider(provType, NULL, flags, NULL, &provNameSize);
	ok(result && provNameSize==cbProvName, "expected %ld, got %ld\n", cbProvName, provNameSize);
	provNameSize = cbProvName;
	
	if (!(provName = LocalAlloc(LMEM_ZEROINIT, provNameSize)))
		return;
	
	result = CryptGetDefaultProvider(provType, NULL, flags, provName, &provNameSize);
	ok(result && !strcmp(pszProvName, provName), "expected %s, got %s\n", pszProvName, provName);
	ok(result && provNameSize==cbProvName, "expected %ld, got %ld\n", cbProvName, provNameSize);

	LocalFree(provName);
}

static void test_set_provider_ex()
{
	DWORD result;
	DWORD notNull = 5;
	
	/* results */
	LPSTR pszProvName = NULL;
	DWORD cbProvName;
	
	/* check pdwReserved for NULL */
	result = CryptSetProviderEx(MS_DEF_PROV, PROV_RSA_FULL, &notNull, CRYPT_MACHINE_DEFAULT);
	ok(!result && GetLastError()==ERROR_INVALID_PARAMETER, "expected %i, got %ld\n",
		ERROR_INVALID_PARAMETER, GetLastError());

	/* remove the default provider and then set it to MS_DEF_PROV/PROV_RSA_FULL */
	result = CryptSetProviderEx(MS_DEF_PROV, PROV_RSA_FULL, NULL, CRYPT_MACHINE_DEFAULT | CRYPT_DELETE_DEFAULT);
	ok(result, "%ld\n", GetLastError());

	result = CryptSetProviderEx(MS_DEF_PROV, PROV_RSA_FULL, NULL, CRYPT_MACHINE_DEFAULT);
	ok(result, "%ld\n", GetLastError());
	
	/* call CryptGetDefaultProvider to see if they match */
	result = CryptGetDefaultProvider(PROV_RSA_FULL, NULL, CRYPT_MACHINE_DEFAULT, NULL, &cbProvName);
	if (!(pszProvName = LocalAlloc(LMEM_ZEROINIT, cbProvName)))
		return;

	result = CryptGetDefaultProvider(PROV_RSA_FULL, NULL, CRYPT_MACHINE_DEFAULT, pszProvName, &cbProvName);
	ok(result && !strcmp(MS_DEF_PROV, pszProvName), "expected %s, got %s\n", MS_DEF_PROV, pszProvName);
	ok(result && cbProvName==(strlen(MS_DEF_PROV) + 1), "expected %i, got %ld\n", (strlen(MS_DEF_PROV) + 1), cbProvName);

	LocalFree(pszProvName);
}

START_TEST(crypt)
{
	init_environment();
	test_acquire_context();
	clean_up_environment();
	
	test_enum_providers();
	test_enum_provider_types();
	test_get_default_provider();
	test_set_provider_ex();
	test_set_provider_ex();
}
