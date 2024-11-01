/*************************************************************
 **
 ** file: read_generic.c
 **
 ** Written by B. M. Bolstad <bmb@bmbolstad.com>
 **
 ** Aim is to read in Affymetrix files in the
 ** "Command Console Generic Data" File Format
 ** This format is sometimes known as the Calvin format
 **
 ** As with other file format functionality in affyio
 ** gzipped files are accepted.
 **
 ** The implementation here is based upon openly available 
 ** file format information. The code here is not dependent or based
 ** in anyway on that in the Fusion SDK.
 **
 **
 ** History
 ** Aug 25, 2007 - Initial version
 ** Sep 9, 2007  - fix some compiler warnings.
 ** Oct 25, 2007 - fix error in decode_UINT8_t
 ** Jan 28, 2008 - fix read_generic_data_group/gzread_generic_data_group. Change bitwise OR (|) to logical OR (||)
 ** Feb 11, 2008 - add #include for inttypes.h in situations that stdint.h might not exist
 ** Feb 13, 2008 - add decode_MIME_value_toASCII which takes any MIME and attempts to convert to a string
 ** Jul 29, 2008 - fix preprocessor directive error for WORDS_BIGENDIAN systems 
 ** Jan 15, 2008 - Fix VECTOR_ELT/STRING_ELT issues
 ** Feb, 2011 - Some debugging code for checking Generic file format parsing
 ** Nov, 2011 - Some additional fixed to deal with fixed width fields for strings in dataset rows
 ** Sept 4, 2017 - change gzFile * to gzFile
 ** August 26, 2021 - Handling fixed width strings of length 0, Better handling for situations where logical ordering and physical ordering of data groups do not agree
 **
 *************************************************************/

#include <R.h>
#include <Rdefines.h>
#include <Rmath.h>
#include <Rinternals.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#elif HAVE_INTTYPES_H
#include <inttypes.h>
#endif

#include <wchar.h>

#include <stdio.h>
#include <stdlib.h>


#include "fread_functions.h"

#include "read_generic.h"


static void Free_ASTRING(ASTRING *string){
  R_Free(string->value);
  string->len =0;
}


static void Free_AWSTRING(AWSTRING *string){
  R_Free(string->value);
  string->len =0;


}


static void Free_nvt_triplet(nvt_triplet *triplet){
  Free_AWSTRING(&(triplet->name));
  Free_ASTRING(&(triplet->value));
  Free_AWSTRING(&(triplet->type));
}


static void Free_nvts_triplet(col_nvts_triplet *triplet){
  Free_AWSTRING(&(triplet->name));

}





void Free_generic_data_header(generic_data_header *header){
  
  int i;
  generic_data_header *temp;

  Free_ASTRING(&(header->data_type_id));
  Free_ASTRING(&(header->unique_file_id));
  Free_AWSTRING(&(header->Date_time));
  Free_AWSTRING(&(header->locale));
  
  for (i =0; i <  header->n_name_type_value; i++){
    Free_nvt_triplet(&(header->name_type_value[i]));
  }
  R_Free(header->name_type_value);
    
  for (i=0; i < (header->n_parent_headers); i++){
    temp = (generic_data_header *)header->parent_headers[i];
    Free_generic_data_header(temp);
    R_Free(temp);
  }
  if (header->parent_headers != 0)
    R_Free(header->parent_headers);


}


void Free_generic_data_group(generic_data_group *data_group){


  Free_AWSTRING(&(data_group->data_group_name));

}


void Free_generic_data_set(generic_data_set *data_set){

  int j,i;

  for (j= 0; j < data_set->ncols; j++){

    if (data_set->col_name_type_value[j].type == 7){  
         for (i=0; i < data_set->nrows; i++){
        /* ASTRING */
        Free_ASTRING(&((ASTRING *)data_set->Data[j])[i]);
	}	
    } else if (data_set->col_name_type_value[j].type == 8){  
         for (i=0; i < data_set->nrows; i++){
        /* AWSTRING */
        Free_AWSTRING(&((AWSTRING *)data_set->Data[j])[i]);
	}	
    }
    R_Free(data_set->Data[j]);
  }
  R_Free(data_set->Data);

  for (j=0; j < data_set->ncols; j++){
    Free_nvts_triplet(&(data_set->col_name_type_value[j]));
  }
  R_Free(data_set->col_name_type_value);

  for (j =0; j <  data_set->n_name_type_value; j++){
    Free_nvt_triplet(&(data_set->name_type_value[j]));
  }
  R_Free(data_set->name_type_value);

  Free_AWSTRING(&(data_set->data_set_name));
  
}
  

static int fread_ASTRING(ASTRING *destination, FILE *instream){

  fread_be_int32(&(destination->len),1,instream);
  if (destination->len > 0){
    destination->value = R_Calloc(destination->len+1,char);
    fread_be_char(destination->value,destination->len,instream);
  } else {
    destination->value = 0;
  }
  return 1;
}



static int fread_ASTRING_fw(ASTRING *destination, FILE *instream, int length){

  fread_be_int32(&(destination->len),1,instream);
  if (destination->len > 0){
    destination->value = R_Calloc(destination->len+1,char);
    fread_be_char(destination->value,destination->len,instream);
    if (length > destination->len){
	fseek(instream, length-destination->len, SEEK_CUR);
    }
  } else {
    destination->value = 0;
    fseek(instream, length, SEEK_CUR);
  }
  return 1;
}


static int fread_AWSTRING(AWSTRING *destination, FILE *instream){

  uint16_t temp;   /* Affy file wchar_t are 16 bits, the platform may have  32 bit wchar_t (notatbly linux) */

  int i;

  fread_be_int32(&(destination->len),1,instream);
  if ((destination->len) > 0){
    destination->value = R_Calloc(destination->len+1,wchar_t);
  
    for (i=0; i < destination->len; i++){
      fread_be_uint16(&temp,1,instream);
      destination->value[i] = (wchar_t)temp;
    }
  } else {
    destination->value = 0;
  }
  
  return 1;
}


static int fread_AWSTRING_fw(AWSTRING *destination, FILE *instream, int length){

  uint16_t temp;   /* Affy file wchar_t are 16 bits, the platform may have  32 bit wchar_t (notatbly linux) */

  int i;

  fread_be_int32(&(destination->len),1,instream);
  if ((destination->len) > 0){
    destination->value = R_Calloc(destination->len+1,wchar_t);
  
    for (i=0; i < destination->len; i++){
      fread_be_uint16(&temp,1,instream);
      destination->value[i] = (wchar_t)temp;
    }  
    if (length > 2*destination->len){
	fseek(instream, length-2*destination->len, SEEK_CUR);
    }
  } else {
    destination->value = 0;
    fseek(instream, length, SEEK_CUR);
  }
  
  return 1;
}







static int fread_nvt_triplet(nvt_triplet *destination, FILE *instream){

  if (!(fread_AWSTRING(&(destination->name),instream)) ||
      !(fread_ASTRING(&(destination->value),instream)) ||
      !fread_AWSTRING(&(destination->type),instream)){
    return 0;
  }
  return 1;
}


static int fread_nvts_triplet(col_nvts_triplet *destination, FILE *instream){

  if (!(fread_AWSTRING(&(destination->name),instream)) ||
      !(fread_be_uchar(&(destination->type), 1, instream)) ||
      !(fread_be_int32(&(destination->size), 1, instream))){
    return 0;
  }
  return 1;
}


/* The Value is MIME text/ASCII */

static char *decode_ASCII(ASTRING value){
  
  char *return_value;

  return_value = R_Calloc(value.len+1,char);
  
  memcpy(return_value, value.value, value.len);


  return return_value;
}



/* The value is MIME text/plain which means wchar (16bit) string */


static wchar_t *decode_TEXT(ASTRING value){

  int i;

  uint32_t len = value.len/ sizeof(uint16_t);
  wchar_t* return_value = R_Calloc(len+1,wchar_t);
  ASTRING temp;
  uint16_t *contents;

  temp.len = value.len;
  temp.value = R_Calloc(value.len, char);
  memcpy(temp.value, value.value,value.len);
  
  contents = (uint16_t *)temp.value;
  
  for (i=0; i < len; i++){
#ifndef WORDS_BIGENDIAN 
    contents[i]=(((contents[i]>>8)&0xff) | ((contents[i]&0xff)<<8));
#endif
    return_value[i] = contents[i];
  }
  Free_ASTRING(&temp);

  return return_value;
}


static int8_t decode_INT8_t(ASTRING value){

  int32_t contents;
  
  memcpy(&contents,value.value, sizeof(int32_t));

  #ifndef WORDS_BIGENDIAN 
    contents=(((contents>>24)&0xff));
  #endif 

  return (int8_t)contents;

}


static uint8_t decode_UINT8_t(ASTRING value){

  uint32_t contents;
  
  memcpy(&contents,value.value, sizeof(uint32_t));

  #ifndef WORDS_BIGENDIAN 
    contents=(((contents>>24)&0xff));
  #endif 

  return (uint8_t)contents;

}


static int16_t decode_INT16_t(ASTRING value){

  int32_t contents;
  
  memcpy(&contents,value.value, sizeof(int32_t));

#ifndef WORDS_BIGENDIAN 
  contents=(((contents>>24)&0xff) | ((contents>>8)&0xff00));
#endif 


  return (int16_t)contents;

}


static uint16_t decode_UINT16_t(ASTRING value){

  uint32_t contents;
  
  memcpy(&contents,value.value, sizeof(uint32_t));

#ifndef WORDS_BIGENDIAN 
  contents=(((contents>>24)&0xff) | ((contents>>8)&0xff00));
#endif 
  return (uint16_t)contents;

}




static int32_t decode_INT32_t(ASTRING value){

  int32_t contents;
  
  memcpy(&contents,value.value, sizeof(int32_t));

#ifndef WORDS_BIGENDIAN 
  contents=(((contents>>24)&0xff) | ((contents&0xff)<<24) |
		((contents>>8)&0xff00) | ((contents&0xff00)<<8));  
#endif 


  return contents;

}

static int32_t decode_UINT32_t(ASTRING value){

  uint32_t contents;
  
  memcpy(&contents,value.value, sizeof(uint32_t));

#ifndef WORDS_BIGENDIAN 
  contents=(((contents>>24)&0xff) | ((contents&0xff)<<24) |
		((contents>>8)&0xff00) | ((contents&0xff00)<<8));  
#endif 


  return contents;

}

static float decode_float32(ASTRING value){


  uint32_t contents;
  float returnvalue;

  memcpy(&contents,value.value, sizeof(uint32_t));

#ifndef WORDS_BIGENDIAN 
  contents=(((contents>>24)&0xff) | ((contents&0xff)<<24) |
	    ((contents>>8)&0xff00) | ((contents&0xff00)<<8));  
#endif 

  memcpy(&returnvalue,&contents, sizeof(uint32_t));
  
  return returnvalue;


}




AffyMIMEtypes determine_MIMETYPE(nvt_triplet triplet){                      
  if (!wcscmp(triplet.type.value,L"text/x-calvin-float")){
    return FLOAT32;
  }
  
  if (!wcscmp(triplet.type.value,L"text/plain")){
    return PLAINTEXT;
  }
  if (!wcscmp(triplet.type.value,L"text/ascii")){
    return ASCIITEXT;
  }
  if (!wcscmp(triplet.type.value,L"text/x-calvin-integer-32")){
    return INT32;
  }

  if (!wcscmp(triplet.type.value,L"text/x-calvin-integer-16")){
    return INT16;
  }

  if (!wcscmp(triplet.type.value,L"text/x-calvin-unsigned-integer-32")){
    return UINT32;
  }

  if (!wcscmp(triplet.type.value,L"text/x-calvin-unsigned-integer-16")){
    return INT16;
  }

  if (!wcscmp(triplet.type.value,L"text/x-calvin-integer-8")){
    return INT8;
  }
  if (!wcscmp(triplet.type.value,L"text/x-calvin-unsigned-integer-8")){
    return UINT8;
  }
  Rprintf("read_generic.c: Unknown MIME type encountered\n");
  return FLOAT32;
}






void *decode_MIME_value(nvt_triplet triplet, AffyMIMEtypes mimetype, void *result, int *size){

  char *temp;
  wchar_t *temp2;

  if (mimetype == ASCIITEXT){
    temp = decode_ASCII(triplet.value);
    *size = strlen(temp);
    result = temp;
    return temp;
  }

  if (mimetype == PLAINTEXT){
    temp2 = decode_TEXT(triplet.value);
    *size = wcslen(temp2);
    result = temp2;
    return temp2;
  }

  if (mimetype == UINT8){
    *size =1;
    *(uint8_t *)result = decode_UINT8_t(triplet.value);
  }
  
  if (mimetype == INT8){ 
    *size =1;
    *(int8_t *)result = decode_INT8_t(triplet.value);
  }
  
  if (mimetype == UINT16){ 
    *size =1;
    *(uint16_t *)result = decode_UINT16_t(triplet.value);
  }
  
  if (mimetype == INT16){ 
    *size =1;
    *(int16_t *)result = decode_INT16_t(triplet.value);
  }
 
  if (mimetype == UINT32){ 
    *size =1;
    *(uint32_t *)result = decode_UINT32_t(triplet.value);
  }
  
  if (mimetype == INT32){ 
    *size =1;
    *(int32_t *)result = decode_INT32_t(triplet.value);
  }
 
  if (mimetype == FLOAT32){ 
    *size =1;
    *(float *)result = decode_float32(triplet.value);
  }
  return 0;
}



char *decode_MIME_value_toASCII(nvt_triplet triplet, AffyMIMEtypes mimetype, void *result, int *size){

  char *temp;
  wchar_t *temp2;
 

  float temp_float;
  uint8_t temp_uint8;
  uint16_t temp_uint16;
  uint32_t temp_uint32;
  int8_t temp_int8;
  int16_t temp_int16;
  int32_t temp_int32;

  

  if (mimetype == ASCIITEXT){
    temp = decode_ASCII(triplet.value);
    *size = strlen(temp);
    result = temp;
    return temp;
  }

  if (mimetype == PLAINTEXT){
    temp2 = decode_TEXT(triplet.value);
    temp = R_Calloc(triplet.value.len/2 +1, char);
    wcstombs(temp,temp2,triplet.value.len/2 + 1);
    *size = strlen(temp);
    result = temp;
    return temp;
  }


  /* 64 here is a bit hackish */
  temp = R_Calloc(64,char);
  if (mimetype == UINT8){
    temp_uint8 = decode_UINT8_t(triplet.value);
    sprintf(temp,"%u",temp_uint8);
    *size = strlen(temp);
    result = temp;
    return temp;
  }
  
  if (mimetype == INT8){ 
    temp_int8 = decode_INT8_t(triplet.value);
    sprintf(temp,"%d",temp_int8);
    *size = strlen(temp);
    result = temp;
    return temp;
  }
  
  if (mimetype == UINT16){ 
    temp_uint16 = decode_UINT16_t(triplet.value);
    sprintf(temp,"%u",temp_uint16);
    *size = strlen(temp);
    result = temp;
    return temp;
  }
  
  if (mimetype == INT16){ 
    temp_int16 = decode_INT16_t(triplet.value);
    sprintf(temp,"%d",temp_int16);
    *size = strlen(temp);
    result = temp;
    return temp;
  }
 
  if (mimetype == UINT32){ 
    temp_uint32 = decode_UINT32_t(triplet.value);
    sprintf(temp,"%u",temp_uint32);
    *size = strlen(temp);
    result = temp;
    return temp;
  }
  
  if (mimetype == INT32){ 
    temp_int32 = decode_INT32_t(triplet.value);
    sprintf(temp,"%d",temp_int32);
    *size = strlen(temp);
    result = temp;
    return temp;
  }
 
  if (mimetype == FLOAT32){ 
    temp_float = decode_float32(triplet.value);
    sprintf(temp,"%f",temp_float);
    *size = strlen(temp);
    result = temp;
    return temp;
  }
  return 0;
}





















nvt_triplet* find_nvt(generic_data_header *data_header,char *name){

  nvt_triplet* returnvalue = 0;

  wchar_t *wname;
  int i;
  
  int len = strlen(name);
  


  wname = R_Calloc(len+1, wchar_t);


  mbstowcs(wname, name, len);

  for (i =0; i < data_header->n_name_type_value; i++){
    if (wcscmp(wname, data_header->name_type_value[i].name.value) == 0){
      returnvalue = &(data_header->name_type_value[i]);
      break;
    }
  }
  
  if (returnvalue == 0){
    for (i =0; i < data_header->n_parent_headers; i++){
      returnvalue = find_nvt((generic_data_header *)(data_header->parent_headers)[i],name);
      if (returnvalue !=0){
	break;
      }
    }
  }
  
  R_Free(wname);
  return returnvalue;
}



int read_generic_file_header(generic_file_header* file_header, FILE *instream){

  if (!fread_be_uchar(&(file_header->magic_number),1,instream)){
    return 0;
  }
  if (file_header->magic_number != 59){
    return 0;
  }
  
  if (!fread_be_uchar(&(file_header->version),1,instream)){
    return 0;
  }

  if (file_header->version != 1){
    return 0;
  }
  
  if (!fread_be_int32(&(file_header->n_data_groups),1,instream) ||
      !fread_be_uint32(&(file_header->first_group_file_pos),1,instream)){
    return 0;
  }

  return 1;
}





int read_generic_data_header(generic_data_header *data_header, FILE *instream){
  
  int i;
  generic_data_header *temp_header;
  

  if (!fread_ASTRING(&(data_header->data_type_id), instream) ||
      !fread_ASTRING(&(data_header->unique_file_id), instream) ||
      !fread_AWSTRING(&(data_header->Date_time), instream) ||
      !fread_AWSTRING(&(data_header->locale),instream)){
    return 0;
  }

  if (!fread_be_int32(&(data_header->n_name_type_value),1,instream)){
    return 0;
  }
  
  data_header->name_type_value = R_Calloc(data_header->n_name_type_value, nvt_triplet);

  for (i =0; i < data_header->n_name_type_value; i++){
    if (!fread_nvt_triplet(&data_header->name_type_value[i],instream)){
      return 0;
    }
  }
  
  if (!fread_be_int32(&(data_header->n_parent_headers),1,instream)){
    return 0;
  }
  
  if (data_header->n_parent_headers > 0){
    data_header->parent_headers = R_Calloc(data_header->n_parent_headers,void *);
  } else {
    data_header->parent_headers = 0;
  }
  for (i =0; i < data_header->n_parent_headers; i++){
    temp_header = (generic_data_header *)R_Calloc(1,generic_data_header);
    if (!read_generic_data_header(temp_header,instream)){
      return 0;
    }
    data_header->parent_headers[i] = temp_header;
  }
  return 1;
}



int read_generic_data_group(generic_data_group *data_group, FILE *instream){
  
  if (!fread_be_uint32(&(data_group->file_position_nextgroup),1,instream) ||
      !fread_be_uint32(&(data_group->file_position_first_data),1,instream) ||
      !fread_be_int32(&(data_group->n_data_sets),1,instream) ||
      !fread_AWSTRING(&(data_group->data_group_name), instream)){
    return 0;
  }
  return 1;

}



int read_generic_data_set(generic_data_set *data_set, FILE *instream){

  int i;

  if (!fread_be_uint32(&(data_set->file_pos_first),1,instream) ||
      !fread_be_uint32(&(data_set->file_pos_last),1,instream) ||
      !fread_AWSTRING(&(data_set->data_set_name), instream) ||
      !fread_be_int32(&(data_set->n_name_type_value),1,instream)){
    return 0;
  }
  
    
  data_set->name_type_value = R_Calloc(data_set->n_name_type_value, nvt_triplet);

  for (i =0; i < data_set->n_name_type_value; i++){
    if (!fread_nvt_triplet(&data_set->name_type_value[i],instream)){
      return 0;
    }
  }

  if (!fread_be_uint32(&(data_set->ncols),1,instream)){
    return 0;
  }
  
  data_set->col_name_type_value = R_Calloc(data_set->ncols,col_nvts_triplet);

  for (i =0; i < data_set->ncols; i++){
    if (!fread_nvts_triplet(&data_set->col_name_type_value[i], instream)){
      return 0;
    }
  }

  if (!fread_be_uint32(&(data_set->nrows),1,instream)){
    return 0;
  }

  data_set->Data = R_Calloc(data_set->ncols, void *);

  for (i=0; i < data_set->ncols; i++){
    switch(data_set->col_name_type_value[i].type){
    case 0: data_set->Data[i] = R_Calloc(data_set->nrows,char);
      break;
    case 1: data_set->Data[i] = R_Calloc(data_set->nrows,unsigned char);
      break;
    case 2: data_set->Data[i] = R_Calloc(data_set->nrows,short);
      break;
    case 3: data_set->Data[i] = R_Calloc(data_set->nrows,unsigned short);
      break;
    case 4: data_set->Data[i] = R_Calloc(data_set->nrows,int);
      break;
    case 5: data_set->Data[i] = R_Calloc(data_set->nrows,unsigned int);
      break;
    case 6: data_set->Data[i] = R_Calloc(data_set->nrows,float);
      break;
/*    case 7: data_set->Data[i] = R_Calloc(data_set->nrows,double);
      break; */
    case 7: data_set->Data[i] = R_Calloc(data_set->nrows,ASTRING);
      break;
    case 8: data_set->Data[i] = R_Calloc(data_set->nrows,AWSTRING);
      break;
    }
    
  }
  return 1;
}


int read_generic_data_set_rows(generic_data_set *data_set, FILE *instream){

  int i,j;
  
  for (i=0; i < data_set->nrows; i++){
    for (j=0; j < data_set->ncols; j++){
      switch(data_set->col_name_type_value[j].type){
      case 0: 
	if (!fread_be_char(&((char *)data_set->Data[j])[i],1,instream)){
	  return 0;
	} 
	break;
      case 1: 
	if (!fread_be_uchar(&((unsigned char *)data_set->Data[j])[i],1,instream)){
	  return 0;
	}  
	break;
      case 2: 	
	if (!fread_be_int16(&((short *)data_set->Data[j])[i],1,instream)){
	  return 0;
	}  
	break;
      case 3:
 	if (!fread_be_uint16(&((unsigned short *)data_set->Data[j])[i],1,instream)){
	  return 0;
	}  
	break;
      case 4:
 	if (!fread_be_int32(&((int32_t *)data_set->Data[j])[i],1,instream)){
	  return 0;
	}  
	break;
      case 5: 	
	if (!fread_be_uint32(&((uint32_t *)data_set->Data[j])[i],1,instream)){
	  return 0;
	}  
	break;
      case 6: 	
	if (!fread_be_float32(&((float *)data_set->Data[j])[i],1,instream)){
	  return 0;
	} 
	break;
/*      case 7: 	
	if (!fread_be_double64(&((double *)data_set->Data[j])[i],1,instream)){
	  return 0;
	} 
	break; */
      case 7: 	
	if (!fread_ASTRING_fw(&((ASTRING *)data_set->Data[j])[i], instream, data_set->col_name_type_value[j].size-4)){
	  return 0;
	} 
	break;
      case 8: 	
	if (!fread_AWSTRING_fw(&((AWSTRING *)data_set->Data[j])[i], instream, data_set->col_name_type_value[j].size-4)){
	  return 0;
	};
	break;
      }
    }
  }
  return 1;
}




/*****************************************************************************
 **
 **
 ** Functionality for reading a generic format file which has been gzipped
 **
 **
 *****************************************************************************/



static int gzread_ASTRING(ASTRING *destination, gzFile instream){

  gzread_be_int32(&(destination->len),1,instream);
  if (destination->len > 0){
    destination->value = R_Calloc(destination->len+1,char);
    gzread_be_char(destination->value,destination->len,instream);
  } else {
    destination->value = 0;
  }
  return 1;
}



static int gzread_ASTRING_fw(ASTRING *destination, gzFile instream, int length){

  gzread_be_int32(&(destination->len),1,instream);
  if (destination->len > 0){
    destination->value = R_Calloc(destination->len+1,char);
    gzread_be_char(destination->value,destination->len,instream);  
    if (length > destination->len){
	gzseek(instream, length-destination->len, SEEK_CUR);
    }
  } else {
    destination->value = 0;
    gzseek(instream, length, SEEK_CUR);
  }
  return 1;
}


static int gzread_AWSTRING(AWSTRING *destination, gzFile instream){

  uint16_t temp;   /* Affy file wchar_t are 16 bits, the platform may have  32 bit wchar_t (notatbly linux) */

  int i;

  gzread_be_int32(&(destination->len),1,instream);
  if ((destination->len) > 0){
    destination->value = R_Calloc(destination->len+1,wchar_t);
  
    for (i=0; i < destination->len; i++){
      gzread_be_uint16(&temp,1,instream);
      destination->value[i] = (wchar_t)temp;
    }
  } else {
    destination->value = 0;
  }
  
  return 1;
}


static int gzread_AWSTRING_fw(AWSTRING *destination, gzFile instream, int length){

  uint16_t temp;   /* Affy file wchar_t are 16 bits, the platform may have  32 bit wchar_t (notatbly linux) */

  int i;

  gzread_be_int32(&(destination->len),1,instream);
  if ((destination->len) > 0){
    destination->value = R_Calloc(destination->len+1,wchar_t);
  
    for (i=0; i < destination->len; i++){
      gzread_be_uint16(&temp,1,instream);
      destination->value[i] = (wchar_t)temp;
    }    
    if (length > 2*destination->len){
	gzseek(instream, length-2*destination->len, SEEK_CUR);
    }

  } else {
    destination->value = 0;
    gzseek(instream, length, SEEK_CUR);
  }
  
  return 1;
}


static int gzread_nvt_triplet(nvt_triplet *destination, gzFile instream){

  if (!(gzread_AWSTRING(&(destination->name),instream)) ||
      !(gzread_ASTRING(&(destination->value),instream)) ||
      !(gzread_AWSTRING(&(destination->type),instream))){
    return 0;
  }
  return 1;
}



static int gzread_nvts_triplet(col_nvts_triplet *destination, gzFile instream){

  if (!(gzread_AWSTRING(&(destination->name),instream)) ||
      !(gzread_be_uchar(&(destination->type), 1, instream)) ||
      !(gzread_be_int32(&(destination->size), 1, instream))){
    return 0;
  }
  return 1;
}


int gzread_generic_file_header(generic_file_header* file_header, gzFile instream){

  if (!gzread_be_uchar(&(file_header->magic_number),1,instream)){
    return 0;
  }
  if (file_header->magic_number != 59){
    return 0;
  }
  
  if (!gzread_be_uchar(&(file_header->version),1,instream)){
    return 0;
  }

  if (file_header->version != 1){
    return 0;
  }
  
  if (!gzread_be_int32(&(file_header->n_data_groups),1,instream) ||
      !gzread_be_uint32(&(file_header->first_group_file_pos),1,instream)){
    return 0;
  }

  return 1;
}




int gzread_generic_data_header(generic_data_header *data_header, gzFile instream){
  
  int i;

  if (!gzread_ASTRING(&(data_header->data_type_id), instream) ||
      !gzread_ASTRING(&(data_header->unique_file_id), instream) ||
      !gzread_AWSTRING(&(data_header->Date_time), instream) ||
      !gzread_AWSTRING(&(data_header->locale),instream)){
    return 0;
  }

  if (!gzread_be_int32(&(data_header->n_name_type_value),1,instream)){
    return 0;
  }
  
  data_header->name_type_value = R_Calloc(data_header->n_name_type_value, nvt_triplet);

  for (i =0; i < data_header->n_name_type_value; i++){
    if (!gzread_nvt_triplet(&data_header->name_type_value[i],instream)){
      return 0;
    }
  }
  
  if (!gzread_be_int32(&(data_header->n_parent_headers),1,instream)){
    return 0;
  }
  
  data_header->parent_headers = R_Calloc(data_header->n_parent_headers,void *);

  for (i =0; i < data_header->n_parent_headers; i++){
    data_header->parent_headers[i] = (generic_data_header *)R_Calloc(1,generic_data_header);
    if (!gzread_generic_data_header((generic_data_header *)data_header->parent_headers[i],instream)){
      return 0;
    }
  }
  return 1;
}





int gzread_generic_data_group(generic_data_group *data_group, gzFile instream){
  
  if (!gzread_be_uint32(&(data_group->file_position_nextgroup),1,instream) ||
      !gzread_be_uint32(&(data_group->file_position_first_data),1,instream) ||
      !gzread_be_int32(&(data_group->n_data_sets),1,instream) ||
      !gzread_AWSTRING(&(data_group->data_group_name), instream)){
    return 0;
  }
  return 1;

}





int gzread_generic_data_set(generic_data_set *data_set, gzFile instream){

  int i;

  if (!gzread_be_uint32(&(data_set->file_pos_first),1,instream) ||
      !gzread_be_uint32(&(data_set->file_pos_last),1,instream) ||
      !gzread_AWSTRING(&(data_set->data_set_name), instream) ||
      !gzread_be_int32(&(data_set->n_name_type_value),1,instream)){
    return 0;
  }
  
    
  data_set->name_type_value = R_Calloc(data_set->n_name_type_value, nvt_triplet);

  for (i =0; i < data_set->n_name_type_value; i++){
    if (!gzread_nvt_triplet(&data_set->name_type_value[i],instream)){
      return 0;
    }
  }

  if (!gzread_be_uint32(&(data_set->ncols),1,instream)){
    return 0;
  }
  
  data_set->col_name_type_value = R_Calloc(data_set->ncols,col_nvts_triplet);

  for (i =0; i < data_set->ncols; i++){
    if (!gzread_nvts_triplet(&data_set->col_name_type_value[i], instream)){
      return 0;
    }
  }

  if (!gzread_be_uint32(&(data_set->nrows),1,instream)){
    return 0;
  }

  data_set->Data = R_Calloc(data_set->ncols, void *);

  for (i=0; i < data_set->ncols; i++){
    switch(data_set->col_name_type_value[i].type){
    case 0: data_set->Data[i] = R_Calloc(data_set->nrows,char);
      break;
    case 1: data_set->Data[i] = R_Calloc(data_set->nrows,unsigned char);
      break;
    case 2: data_set->Data[i] = R_Calloc(data_set->nrows,short);
      break;
    case 3: data_set->Data[i] = R_Calloc(data_set->nrows,unsigned short);
      break;
    case 4: data_set->Data[i] = R_Calloc(data_set->nrows,int);
      break;
    case 5: data_set->Data[i] = R_Calloc(data_set->nrows,unsigned int);
      break;
    case 6: data_set->Data[i] = R_Calloc(data_set->nrows,float);
      break;
/*    case 7: data_set->Data[i] = R_Calloc(data_set->nrows,double);
      break; */
    case 7: data_set->Data[i] = R_Calloc(data_set->nrows,ASTRING);
      break;
    case 8: data_set->Data[i] = R_Calloc(data_set->nrows,AWSTRING);
      break;
    }
    
  }
  return 1;
}




int gzread_generic_data_set_rows(generic_data_set *data_set, gzFile instream){

  int i,j;
  
  for (i=0; i < data_set->nrows; i++){
    for (j=0; j < data_set->ncols; j++){
      switch(data_set->col_name_type_value[j].type){
      case 0: 
	if (!gzread_be_char(&((char *)data_set->Data[j])[i],1,instream)){
	  return 0;
	} 
	break;
      case 1: 
	if (!gzread_be_uchar(&((unsigned char *)data_set->Data[j])[i],1,instream)){
	  return 0;
	}  
	break;
      case 2: 	
	if (!gzread_be_int16(&((short *)data_set->Data[j])[i],1,instream)){
	  return 0;
	}  
	break;
      case 3:
 	if (!gzread_be_uint16(&((unsigned short *)data_set->Data[j])[i],1,instream)){
	  return 0;
	}  
	break;
      case 4:
 	if (!gzread_be_int32(&((int32_t *)data_set->Data[j])[i],1,instream)){
	  return 0;
	}  
	break;
      case 5: 	
	if (!gzread_be_uint32(&((uint32_t *)data_set->Data[j])[i],1,instream)){
	  return 0;
	}  
	break;
      case 6: 	
	if (!gzread_be_float32(&((float *)data_set->Data[j])[i],1,instream)){
	  return 0;
	} 
	break;
/*      case 7: 	
	if (!gzread_be_double64(&((double *)data_set->Data[j])[i],1,instream)){
	  return 0;
	} 
	break; */
      case 7: 	
	if (!gzread_ASTRING_fw(&((ASTRING *)data_set->Data[j])[i], instream,data_set->col_name_type_value[j].size-4)){
	  return 0;
	} 
	break;
      case 8: 	
	if (!gzread_AWSTRING_fw(&((AWSTRING *)data_set->Data[j])[i], instream, data_set->col_name_type_value[j].size-4)){
	  return 0;
	};
	break;
      }
    }
  }
  return 1;
}






/*****************************************************************************
 **
 ** TESTING FUNCTIONS
 **
 **
 **
 **
 ** The following functions are for testing purposes only they print contents
 **  of the  generic format file to the screen
 **
 ******************************************************************************/


static void print_file_header(generic_file_header header){
  Rprintf("Magic Number: %d\n",header.magic_number);
  Rprintf("Header Version: %d\n",header.version);
  Rprintf("Number of DataGroups: %d\n",header.n_data_groups);
  Rprintf("FirstGroup Position: %d\n",header.first_group_file_pos);
}


static void print_ASTRING(ASTRING string){ 
  if (string.len > 0){
    Rprintf("%s",string.value);
  }
}


static void print_AWSTRING(AWSTRING string){
  if (string.len > 0){
    char *temp = R_Calloc(string.len+1,char);
    wcstombs(temp, string.value, string.len);
    
    Rprintf("%s",temp);
    
    
    R_Free(temp);
  }
}

static void print_decode_nvt_triplet(nvt_triplet triplet){
  
  wchar_t *temp;
  char *temp2;
  //  char Buffer[10000];
  int size;

  int temp32;
  float tempfloat;


  //  Rprintf("Size is %d\n",triplet.value.len); 
  if (!wcscmp(triplet.type.value,L"text/x-calvin-float")){
    Rprintf("Its a float  value is %f\n",decode_float32(triplet.value));

    Rprintf("Now Trying it again. But using exposed function\n");
    decode_MIME_value(triplet, determine_MIMETYPE(triplet),&tempfloat,&size);
    Rprintf("Its a float  value is %f\n",temp32);
  }
    
  if (!wcscmp(triplet.type.value,L"text/ascii")){
    temp2 = decode_ASCII(triplet.value);
    Rprintf("Its a Ascii String  value is %s\n",temp2);
    R_Free(temp2);

    Rprintf("Now Trying it again. But using exposed function\n");
    temp2 = decode_MIME_value(triplet, determine_MIMETYPE(triplet),temp2,&size);
    Rprintf("Its a Ascii String  value is %s with size %d\n",temp2, size);
    R_Free(temp2);
  }
  
  if (!wcscmp(triplet.type.value,L"text/plain")){
    temp = decode_TEXT(triplet.value);
    temp2 = R_Calloc(triplet.value.len/2 +1, char);
    wcstombs(temp2,temp,triplet.value.len/2 + 1);
    Rprintf("Text/plain String is %s\n",temp2);
    R_Free(temp);
    R_Free(temp2);

    Rprintf("Now Trying it again. But using exposed function\n");
    
    temp = (wchar_t *)decode_MIME_value(triplet, determine_MIMETYPE(triplet),temp,&size);
    temp2 = R_Calloc(size +1, char);
    wcstombs(temp2,temp,size);
    Rprintf("Its a Text/plain string value is %s with size %d\n",temp2, size);
    R_Free(temp2);
    R_Free(temp);
  }

  if (!wcscmp(triplet.type.value,L"text/x-calvin-integer-32")){
    Rprintf("Its a int32_t  value is %d\n",decode_INT32_t(triplet.value));

    Rprintf("Now Trying it again. But using exposed function\n");
    
    decode_MIME_value(triplet, determine_MIMETYPE(triplet),&temp32,&size);
    Rprintf("Its a int32_t  value is %d\n",temp32);

  }

  if (!wcscmp(triplet.type.value,L"text/x-calvin-integer-16")){
    Rprintf("Its a int16_t  value is %d\n",decode_INT16_t(triplet.value));
  }

  if (!wcscmp(triplet.type.value,L"text/x-calvin-unsigned-integer-32")){
    Rprintf("Its a uint32_t  value is %d\n",decode_UINT32_t(triplet.value));
  }

  if (!wcscmp(triplet.type.value,L"text/x-calvin-unsigned-integer-16")){
    Rprintf("Its a uint16_t  value is %d\n",decode_UINT16_t(triplet.value));
  }

  if (!wcscmp(triplet.type.value,L"text/x-calvin-integer-8")){
    Rprintf("Its a int8_t  value is %d\n",decode_INT8_t(triplet.value));
  }
  if (!wcscmp(triplet.type.value,L"text/x-calvin-unsigned-integer-8")){
    Rprintf("Its a uint8_t  value is %d\n",decode_UINT8_t(triplet.value));
  }
}


static void print_nvt_triplet(nvt_triplet triplet){

  print_AWSTRING(triplet.name);
  Rprintf("  ");
  // print_ASTRING(triplet.value);
  //Rprintf("  ");
  print_AWSTRING(triplet.type);
  //Rprintf("\n"); 
  print_decode_nvt_triplet(triplet);
}

static void print_col_nvts_triplet(col_nvts_triplet triplet){

  print_AWSTRING(triplet.name);
  Rprintf("  %d   %d", triplet.type, triplet.size);
  Rprintf("\n");
}

static void print_generic_header(generic_data_header header){

  int i;

  print_ASTRING(header.data_type_id);
  Rprintf("\n");
  print_ASTRING(header.unique_file_id);
  Rprintf("\n");
  print_AWSTRING(header.Date_time);
  Rprintf("\n");
  print_AWSTRING(header.locale);
  Rprintf("\n");
  Rprintf("%d\n", header.n_name_type_value);

  for (i=0; i < header.n_name_type_value; i++){
    print_nvt_triplet(header.name_type_value[i]);
   
  }

  Rprintf("%d\n",header.n_parent_headers);
  if (header.n_parent_headers > 0){
    Rprintf("Printing Parental Headers\n");
    for (i =0; i < header.n_parent_headers; i++){
      print_generic_header(*(generic_data_header *)header.parent_headers[i]);
    }
  }
}


static void print_generic_data_group(generic_data_group data_group){
  
  Rprintf("%d\n",data_group.file_position_nextgroup);
  Rprintf("%d\n",data_group.file_position_first_data);
  Rprintf("%d\n",data_group.n_data_sets);
  Rprintf("Data Group Name is   :  ");
  print_AWSTRING(data_group.data_group_name);
  Rprintf("\n");
}


static void print_generic_data_set(generic_data_set data_set){
  int i;

  Rprintf("%d\n%d\n",data_set.file_pos_first,data_set.file_pos_last);

  print_AWSTRING(data_set.data_set_name);
  Rprintf("\n");
  Rprintf("%d\n",data_set.n_name_type_value);

  for (i=0; i < data_set.n_name_type_value; i++){
    print_nvt_triplet(data_set.name_type_value[i]);
  }
  
  Rprintf("%d\n",data_set.ncols);
  
  for (i=0; i < data_set.ncols; i++){
    print_col_nvts_triplet(data_set.col_name_type_value[i]);
  }
  Rprintf("%d\n",data_set.nrows);
}



SEXP Read_Generic(SEXP filename){

  int i,j,k;

  SEXP return_value = R_NilValue;

  FILE *infile;

  generic_file_header my_header;
  generic_data_header my_data_header;
  generic_data_group my_data_group;

  generic_data_set my_data_set;

  const char *cur_file_name = CHAR(STRING_ELT(filename,0));

  /* Pass through all the header information */
  
  if ((infile = fopen(cur_file_name, "rb")) == NULL)
    {
      error("Unable to open the file %s\n",cur_file_name);
      return 0;
    }
  

  
  read_generic_file_header(&my_header, infile);
  read_generic_data_header(&my_data_header, infile);
  Rprintf("========= Printing File Header  =========\n");
  print_file_header(my_header);
  Rprintf("========= Printing Generic Header  =========\n");
  print_generic_header(my_data_header);

  for (k =0; k < my_header.n_data_groups; k++){
    Rprintf("========= Printing Data Group  =========\n");
    read_generic_data_group(&my_data_group,infile);
    print_generic_data_group(my_data_group);
    for (j=0; j < my_data_group.n_data_sets; j++){
      read_generic_data_set(&my_data_set,infile); 
      Rprintf("========= Printing Data Set  =========\n");
      print_generic_data_set(my_data_set);
      read_generic_data_set_rows(&my_data_set,infile); 
      for (i =0; i < 1  ; i++){
	//printf("%f\n",((float *)my_data_set.Data[0])[i]);
      }
      // Free_generic_data_set(&my_data_set);
      fseek(infile, my_data_set.file_pos_last, SEEK_SET);
      Free_generic_data_set(&my_data_set);
    }
    fseek(infile, my_data_group.file_position_nextgroup, SEEK_SET);
    Free_generic_data_group(&my_data_group);
  }
  Free_generic_data_header(&my_data_header);


  return return_value;

}


SEXP gzRead_Generic(SEXP filename){

  int i,j,k;

  SEXP return_value = R_NilValue;

  gzFile infile;

  generic_file_header my_header;
  generic_data_header my_data_header;
  generic_data_group my_data_group;

  generic_data_set my_data_set;

  const char *cur_file_name = CHAR(STRING_ELT(filename,0));

  /* Pass through all the header information */
  
  if ((infile = gzopen(cur_file_name, "rb")) == NULL)
    {
      error("Unable to open the file %s\n",cur_file_name);
      return 0;
    }
  

  
  gzread_generic_file_header(&my_header, infile);
  gzread_generic_data_header(&my_data_header, infile);

  Rprintf("========= Printing File Header  =========\n");
  print_file_header(my_header);
  Rprintf("========= Printing Generic Header  =========\n");
  print_generic_header(my_data_header);
  

  for (k =0; k < my_header.n_data_groups; k++){
    Rprintf("========= Printing Data Group  =========\n");
    gzread_generic_data_group(&my_data_group,infile);
    // read_generic_data_set(&my_data_set,infile); 
    //read_generic_data_set_rows(&my_data_set,infile); 
    print_generic_data_group(my_data_group);
    for (j=0; j < my_data_group.n_data_sets; j++){
      gzread_generic_data_set(&my_data_set,infile); 
      Rprintf("========= Printing Data Set  =========\n");
      print_generic_data_set(my_data_set);
      gzread_generic_data_set_rows(&my_data_set,infile); 
      for (i =0; i < 1  ; i++){
	//printf("%f\n",((float *)my_data_set.Data[0])[i]);
      }
      // Free_generic_data_set(&my_data_set);
      gzseek(infile, my_data_set.file_pos_last, SEEK_SET);
      Free_generic_data_set(&my_data_set);
    }
    Free_generic_data_group(&my_data_group);
  }
  Free_generic_data_header(&my_data_header);


  return return_value;

}




static SEXP file_header_R_List(generic_file_header *my_header){

  SEXP return_value, return_names;	
  SEXP tmp_sexp;

  PROTECT(return_value = allocVector(VECSXP,3));	

  PROTECT(tmp_sexp= allocVector(INTSXP,1));
  INTEGER(tmp_sexp)[0] = (int32_t)my_header->magic_number; 
  SET_VECTOR_ELT(return_value,0,tmp_sexp);
  UNPROTECT(1);

   
  PROTECT(tmp_sexp= allocVector(INTSXP,1));
  INTEGER(tmp_sexp)[0] =  (int32_t)my_header->version;
  SET_VECTOR_ELT(return_value,1,tmp_sexp);
  UNPROTECT(1);

  PROTECT(tmp_sexp= allocVector(INTSXP,1));
  INTEGER(tmp_sexp)[0] =  (int32_t)my_header->n_data_groups;
  SET_VECTOR_ELT(return_value,2,tmp_sexp);
  UNPROTECT(1);
  
  PROTECT(return_names = allocVector(STRSXP,3));
  SET_STRING_ELT(return_names,0,mkChar("MagicNumber"));
  SET_STRING_ELT(return_names,1,mkChar("Version"));
  SET_STRING_ELT(return_names,2,mkChar("NumberDataGroups"));
   
  setAttrib(return_value, R_NamesSymbol, return_names); 
  UNPROTECT(2);
  return return_value;
}









static SEXP decode_nvt_triplet(nvt_triplet triplet){
  
  wchar_t *temp=0;
  char *temp2=0;

  int size;

  int temp32;
  float tempfloat;
 
  SEXP return_value=R_NilValue;

  if (!wcscmp(triplet.type.value,L"text/x-calvin-float")){

    decode_MIME_value(triplet, determine_MIMETYPE(triplet),&tempfloat,&size);

    PROTECT(return_value=allocVector(REALSXP,1));
    NUMERIC_POINTER(return_value)[0] = (double)tempfloat;
    UNPROTECT(1);
    return(return_value);	
  } 
  if (!wcscmp(triplet.type.value,L"text/ascii")){
    temp2 = decode_MIME_value(triplet, determine_MIMETYPE(triplet),temp2,&size);
    
    PROTECT(return_value=allocVector(STRSXP,1));
    SET_STRING_ELT(return_value,0,mkChar(temp2));
    UNPROTECT(1);
    R_Free(temp2);
     return(return_value);	
  }
  
  if (!wcscmp(triplet.type.value,L"text/plain")){
    temp = (wchar_t *)decode_MIME_value(triplet, determine_MIMETYPE(triplet),temp,&size);
    temp2 = R_Calloc(size +1, char);
    wcstombs(temp2,temp,size);
 
    PROTECT(return_value=allocVector(STRSXP,1));
    SET_STRING_ELT(return_value,0,mkChar(temp2));
    UNPROTECT(1);

    R_Free(temp2);
    R_Free(temp); 
    return(return_value);	
  }

  if (!wcscmp(triplet.type.value,L"text/x-calvin-integer-32")){
    decode_MIME_value(triplet, determine_MIMETYPE(triplet),&temp32,&size);
    PROTECT(return_value=allocVector(INTSXP,1));
    INTEGER_POINTER(return_value)[0] = (int32_t)temp32;
    UNPROTECT(1);
    return(return_value);	
  }

  if (!wcscmp(triplet.type.value,L"text/x-calvin-integer-16")){
    PROTECT(return_value=allocVector(INTSXP,1));
    INTEGER_POINTER(return_value)[0] = (int32_t)decode_INT16_t(triplet.value);
    UNPROTECT(1); 
    return(return_value);	
  }

  if (!wcscmp(triplet.type.value,L"text/x-calvin-unsigned-integer-32")){
    PROTECT(return_value=allocVector(INTSXP,1));
    INTEGER_POINTER(return_value)[0] = (int32_t)decode_UINT32_t(triplet.value);
    UNPROTECT(1);  
    return(return_value);	
  }

  if (!wcscmp(triplet.type.value,L"text/x-calvin-unsigned-integer-16")){
    PROTECT(return_value=allocVector(INTSXP,1));
    INTEGER_POINTER(return_value)[0] = (int32_t)decode_UINT16_t(triplet.value);
    UNPROTECT(1);  
    return(return_value);	
  }

  if (!wcscmp(triplet.type.value,L"text/x-calvin-integer-8")){
    PROTECT(return_value=allocVector(INTSXP,1));
    INTEGER_POINTER(return_value)[0] = (int32_t)decode_INT8_t(triplet.value);
    UNPROTECT(1);
    return(return_value);	
  }

  if (!wcscmp(triplet.type.value,L"text/x-calvin-unsigned-integer-8")){
    PROTECT(return_value=allocVector(INTSXP,1));
    INTEGER_POINTER(return_value)[0] = (int32_t)decode_UINT8_t(triplet.value);
    UNPROTECT(1);  
    return(return_value);	
  } 
  return(return_value);
}


static SEXP data_header_R_List(generic_data_header *my_data_header){

  SEXP return_value= R_NilValue, return_names= R_NilValue;	
  SEXP tmp_sexp= R_NilValue, tmp_names= R_NilValue;
  char *temp;
  int i;

  PROTECT(return_value = allocVector(VECSXP,8));	

  PROTECT(tmp_sexp= allocVector(STRSXP,1));
  if (my_data_header->data_type_id.len > 0){
    SET_STRING_ELT(tmp_sexp,0,mkChar(my_data_header->data_type_id.value));
  }
  SET_VECTOR_ELT(return_value,0,tmp_sexp);
  UNPROTECT(1); 	

  PROTECT(tmp_sexp= allocVector(STRSXP,1));  
  if (my_data_header->unique_file_id.len > 0){
    SET_STRING_ELT(tmp_sexp,0,mkChar(my_data_header->unique_file_id.value));
  }
  SET_VECTOR_ELT(return_value,1,tmp_sexp);
  UNPROTECT(1); 	

  PROTECT(tmp_sexp= allocVector(STRSXP,1));
  if (my_data_header->Date_time.len > 0){
    temp = R_Calloc(my_data_header->Date_time.len+1,char);
    wcstombs(temp, my_data_header->Date_time.value, my_data_header->Date_time.len);
    SET_STRING_ELT(tmp_sexp,0,mkChar(temp));  
    R_Free(temp);
  }
  SET_VECTOR_ELT(return_value,2,tmp_sexp);
  UNPROTECT(1); 
 
  PROTECT(tmp_sexp= allocVector(STRSXP,1));
  if (my_data_header->locale.len > 0){
    temp = R_Calloc(my_data_header->locale.len+1,char);
    wcstombs(temp, my_data_header->locale.value, my_data_header->locale.len);
    SET_STRING_ELT(tmp_sexp,0,mkChar(temp));  
    R_Free(temp);
  }
  SET_VECTOR_ELT(return_value,3,tmp_sexp);
  UNPROTECT(1); 
   
  PROTECT(tmp_sexp= allocVector(INTSXP,1));
  INTEGER(tmp_sexp)[0] =  (int32_t)my_data_header->n_name_type_value;
  SET_VECTOR_ELT(return_value,4,tmp_sexp);
  UNPROTECT(1); 

  PROTECT(tmp_sexp= allocVector(VECSXP,my_data_header->n_name_type_value));
  PROTECT(tmp_names =  allocVector(STRSXP,my_data_header->n_name_type_value));
  for (i=0; i < my_data_header->n_name_type_value; i++){
     SET_VECTOR_ELT(tmp_sexp,i,decode_nvt_triplet(my_data_header->name_type_value[i]));
     temp = R_Calloc(my_data_header->name_type_value[i].name.len+1,char);
     wcstombs(temp, my_data_header->name_type_value[i].name.value, my_data_header->name_type_value[i].name.len);
     SET_STRING_ELT(tmp_names,i,mkChar(temp));
     R_Free(temp);
  } 
  setAttrib(tmp_sexp, R_NamesSymbol, tmp_names); 
  SET_VECTOR_ELT(return_value,5,tmp_sexp);
  UNPROTECT(2); 

  PROTECT(tmp_sexp= allocVector(INTSXP,1));
  INTEGER(tmp_sexp)[0] =  (int32_t)my_data_header->n_parent_headers;
  SET_VECTOR_ELT(return_value,6,tmp_sexp);
  UNPROTECT(1); 
  
  PROTECT(tmp_sexp= allocVector(VECSXP,my_data_header->n_parent_headers)); 
  if (my_data_header->n_parent_headers > 0){
   for (i =0; i < my_data_header->n_parent_headers; i++){
      SET_VECTOR_ELT(tmp_sexp,i,data_header_R_List(my_data_header->parent_headers[i]));
    }
  }
  SET_VECTOR_ELT(return_value,7,tmp_sexp);
  UNPROTECT(1); 


  PROTECT(return_names = allocVector(STRSXP,8));
  SET_STRING_ELT(return_names,0,mkChar("DataTypeID"));
  SET_STRING_ELT(return_names,1,mkChar("UniqueFileID"));
  SET_STRING_ELT(return_names,2,mkChar("DateTime"));
  SET_STRING_ELT(return_names,3,mkChar("Locale"));
  SET_STRING_ELT(return_names,4,mkChar("NumberOfNameValueType"));
  SET_STRING_ELT(return_names,5,mkChar("NVTList"));
  SET_STRING_ELT(return_names,6,mkChar("NumberOfParentHeaders"));
  SET_STRING_ELT(return_names,7,mkChar("ParentHeaders"));
  setAttrib(return_value, R_NamesSymbol, return_names); 
  UNPROTECT(2);
  return return_value;
}






static SEXP data_header_R_List_full(generic_data_header *my_data_header){

  SEXP return_value= R_NilValue, return_names= R_NilValue;	
  SEXP tmp_sexp= R_NilValue, tmp_names= R_NilValue, tmp_type= R_NilValue, tmp_value= R_NilValue;
  char *temp;
  int i;

  PROTECT(return_value = allocVector(VECSXP,8));	

  PROTECT(tmp_sexp= allocVector(STRSXP,1));
  if (my_data_header->data_type_id.len > 0){
    SET_STRING_ELT(tmp_sexp,0,mkChar(my_data_header->data_type_id.value));
  }
  SET_VECTOR_ELT(return_value,0,tmp_sexp);
  UNPROTECT(1); 	

  PROTECT(tmp_sexp= allocVector(STRSXP,1));  
  if (my_data_header->unique_file_id.len > 0){
    SET_STRING_ELT(tmp_sexp,0,mkChar(my_data_header->unique_file_id.value));
  }
  SET_VECTOR_ELT(return_value,1,tmp_sexp);
  UNPROTECT(1); 	

  PROTECT(tmp_sexp= allocVector(STRSXP,1));
  if (my_data_header->Date_time.len > 0){
    temp = R_Calloc(my_data_header->Date_time.len+1,char);
    wcstombs(temp, my_data_header->Date_time.value, my_data_header->Date_time.len);
    SET_STRING_ELT(tmp_sexp,0,mkChar(temp));  
    R_Free(temp);
  }
  SET_VECTOR_ELT(return_value,2,tmp_sexp);
  UNPROTECT(1); 
 
  PROTECT(tmp_sexp= allocVector(STRSXP,1));
  if (my_data_header->locale.len > 0){
    temp = R_Calloc(my_data_header->locale.len+1,char);
    wcstombs(temp, my_data_header->locale.value, my_data_header->locale.len);
    SET_STRING_ELT(tmp_sexp,0,mkChar(temp));  
    R_Free(temp);
  }
  SET_VECTOR_ELT(return_value,3,tmp_sexp);
  UNPROTECT(1); 
   
  PROTECT(tmp_sexp= allocVector(INTSXP,1));
  INTEGER(tmp_sexp)[0] =  (int32_t)my_data_header->n_name_type_value;
  SET_VECTOR_ELT(return_value,4,tmp_sexp);
  UNPROTECT(1); 

  PROTECT(tmp_sexp= allocVector(VECSXP,3));
  PROTECT(tmp_value = allocVector(VECSXP,  my_data_header->n_name_type_value));
  PROTECT(tmp_names = allocVector(STRSXP, my_data_header->n_name_type_value));
  PROTECT(tmp_type = allocVector(STRSXP, my_data_header->n_name_type_value));
  
  for (i=0; i < my_data_header->n_name_type_value; i++){
     SET_VECTOR_ELT(tmp_value,i,decode_nvt_triplet(my_data_header->name_type_value[i]));
     temp = R_Calloc(my_data_header->name_type_value[i].name.len+1,char);
     wcstombs(temp, my_data_header->name_type_value[i].name.value, my_data_header->name_type_value[i].name.len);
     SET_STRING_ELT(tmp_names,i,mkChar(temp));
     R_Free(temp);
     temp = R_Calloc(my_data_header->name_type_value[i].type.len+1,char);
     wcstombs(temp, my_data_header->name_type_value[i].type.value, my_data_header->name_type_value[i].type.len);
     SET_STRING_ELT(tmp_type,i,mkChar(temp));
     R_Free(temp);

     
  } 
  setAttrib(tmp_value, R_NamesSymbol, tmp_names);
  SET_VECTOR_ELT(tmp_sexp,0,tmp_names);
  SET_VECTOR_ELT(tmp_sexp,1,tmp_value);
  SET_VECTOR_ELT(tmp_sexp,2,tmp_type);

  PROTECT(return_names = allocVector(STRSXP,3));
  SET_STRING_ELT(return_names,0,mkChar("Name"));
  SET_STRING_ELT(return_names,1,mkChar("Value"));
  SET_STRING_ELT(return_names,2,mkChar("Type"));
  setAttrib(tmp_sexp, R_NamesSymbol, return_names); 
  UNPROTECT(1);

  SET_VECTOR_ELT(return_value,5,tmp_sexp);
  UNPROTECT(4); 

  PROTECT(tmp_sexp= allocVector(INTSXP,1));
  INTEGER(tmp_sexp)[0] =  (int32_t)my_data_header->n_parent_headers;
  SET_VECTOR_ELT(return_value,6,tmp_sexp);
  UNPROTECT(1); 
  
  PROTECT(tmp_sexp= allocVector(VECSXP,my_data_header->n_parent_headers)); 
  if (my_data_header->n_parent_headers > 0){
   for (i =0; i < my_data_header->n_parent_headers; i++){
      SET_VECTOR_ELT(tmp_sexp,i,data_header_R_List_full(my_data_header->parent_headers[i]));
    }
  }
  SET_VECTOR_ELT(return_value,7,tmp_sexp);
  UNPROTECT(1); 


  PROTECT(return_names = allocVector(STRSXP,8));
  SET_STRING_ELT(return_names,0,mkChar("DataTypeID"));
  SET_STRING_ELT(return_names,1,mkChar("UniqueFileID"));
  SET_STRING_ELT(return_names,2,mkChar("DateTime"));
  SET_STRING_ELT(return_names,3,mkChar("Locale"));
  SET_STRING_ELT(return_names,4,mkChar("NumberOfNameValueType"));
  SET_STRING_ELT(return_names,5,mkChar("NVTList"));
  SET_STRING_ELT(return_names,6,mkChar("NumberOfParentHeaders"));
  SET_STRING_ELT(return_names,7,mkChar("ParentHeaders"));
  setAttrib(return_value, R_NamesSymbol, return_names); 
  UNPROTECT(2);
  return return_value;
}


static SEXP data_group_R_list(generic_data_group *my_data_group){

  SEXP return_value;
  SEXP tmp_sexp=R_NilValue, return_names= R_NilValue;
  char *temp;

  PROTECT(return_value =  allocVector(VECSXP,2));
  if (my_data_group->data_group_name.len > 0){
     PROTECT(tmp_sexp= allocVector(STRSXP,1)); 
     temp = R_Calloc(my_data_group->data_group_name.len+1,char);
     wcstombs(temp, my_data_group->data_group_name.value, my_data_group->data_group_name.len);
     SET_STRING_ELT(tmp_sexp,0,mkChar(temp));  
     R_Free(temp);
  }	
  SET_VECTOR_ELT(return_value,0,tmp_sexp);
  UNPROTECT(1);
   


  SET_VECTOR_ELT(return_value,1,allocVector(VECSXP,my_data_group->n_data_sets));
  PROTECT(return_names = allocVector(STRSXP,2));
  SET_STRING_ELT(return_names,0,mkChar("Name"));
  SET_STRING_ELT(return_names,1,mkChar("Datasets"));
  setAttrib(return_value, R_NamesSymbol, return_names); 
  UNPROTECT(2);
  return return_value;   
}



static SEXP generic_data_set_R_List(generic_data_set *my_data_set){

  SEXP return_value= R_NilValue, return_names= R_NilValue;
  SEXP tmp_sexp= R_NilValue, tmp_names= R_NilValue;
  int i;
  char *temp;

  PROTECT(return_value =  allocVector(VECSXP,3));
  
  PROTECT(tmp_sexp= allocVector(STRSXP,1));  
  if (my_data_set->data_set_name.len > 0){
    temp = R_Calloc(my_data_set->data_set_name.len+1,char);
    wcstombs(temp, my_data_set->data_set_name.value, my_data_set->data_set_name.len);
    SET_STRING_ELT(tmp_sexp,0,mkChar(temp));  
    R_Free(temp);
  }
  SET_VECTOR_ELT(return_value,0,tmp_sexp);
  UNPROTECT(1);

  PROTECT(tmp_sexp= allocVector(VECSXP,my_data_set->n_name_type_value));
  PROTECT(tmp_names =  allocVector(STRSXP,my_data_set->n_name_type_value));
  for (i=0; i < my_data_set->n_name_type_value; i++){
    //print_nvt_triplet(data_set.name_type_value[i]);
    SET_VECTOR_ELT(tmp_sexp,i,decode_nvt_triplet(my_data_set->name_type_value[i]));
    temp = R_Calloc(my_data_set->name_type_value[i].name.len+1,char);
    wcstombs(temp, my_data_set->name_type_value[i].name.value, my_data_set->name_type_value[i].name.len);
    SET_STRING_ELT(tmp_names,i,mkChar(temp));
    R_Free(temp);
  } 
  setAttrib(tmp_sexp, R_NamesSymbol, tmp_names); 
  SET_VECTOR_ELT(return_value,1,tmp_sexp);
  UNPROTECT(2); 

  PROTECT(tmp_sexp= allocVector(VECSXP,my_data_set->ncols));  
  SET_VECTOR_ELT(return_value,2,tmp_sexp);
  PROTECT(tmp_names =  allocVector(STRSXP,my_data_set->ncols));
  for (i=0; i < my_data_set->ncols; i++){
     temp = R_Calloc(my_data_set->col_name_type_value[i].name.len+1,char);
     wcstombs(temp, my_data_set->col_name_type_value[i].name.value, my_data_set->col_name_type_value[i].name.len);
     SET_STRING_ELT(tmp_names,i,mkChar(temp));
     R_Free(temp);
  }
  setAttrib(tmp_sexp, R_NamesSymbol, tmp_names); 
  UNPROTECT(2);

  PROTECT(return_names = allocVector(STRSXP,3));
  SET_STRING_ELT(return_names,0,mkChar("Name"));
  SET_STRING_ELT(return_names,1,mkChar("NVTList"));
  SET_STRING_ELT(return_names,2,mkChar("DataColumns"));
  setAttrib(return_value, R_NamesSymbol, return_names); 
  UNPROTECT(2);
  return return_value;

}





static SEXP generic_data_set_R_List_full(generic_data_set *my_data_set){

  SEXP return_value = R_NilValue, return_names = R_NilValue;
  SEXP tmp_sexp = R_NilValue, tmp_names = R_NilValue, tmp_type = R_NilValue, tmp_value = R_NilValue, tmp_size = R_NilValue;
  int i;
  char *temp;

  PROTECT(return_value =  allocVector(VECSXP,4));
  
  PROTECT(tmp_sexp= allocVector(STRSXP,1));  
  if (my_data_set->data_set_name.len > 0){
    temp = R_Calloc(my_data_set->data_set_name.len+1,char);
    wcstombs(temp, my_data_set->data_set_name.value, my_data_set->data_set_name.len);
    SET_STRING_ELT(tmp_sexp,0,mkChar(temp));  
    R_Free(temp);
  }
  SET_VECTOR_ELT(return_value,0,tmp_sexp);
  UNPROTECT(1);

  PROTECT(tmp_sexp= allocVector(VECSXP,3));
  PROTECT(tmp_names =  allocVector(STRSXP,my_data_set->n_name_type_value));
  PROTECT(tmp_type =  allocVector(STRSXP,my_data_set->n_name_type_value));
  PROTECT(tmp_value =  allocVector(VECSXP,my_data_set->n_name_type_value));
  for (i=0; i < my_data_set->n_name_type_value; i++){
    SET_VECTOR_ELT(tmp_value,i,decode_nvt_triplet(my_data_set->name_type_value[i]));
    temp = R_Calloc(my_data_set->name_type_value[i].name.len+1,char);
    wcstombs(temp, my_data_set->name_type_value[i].name.value, my_data_set->name_type_value[i].name.len);
    SET_STRING_ELT(tmp_names,i,mkChar(temp));
    R_Free(temp);
    temp = R_Calloc(my_data_set->name_type_value[i].type.len+1,char);
    wcstombs(temp, my_data_set->name_type_value[i].type.value, my_data_set->name_type_value[i].type.len);
    SET_STRING_ELT(tmp_type,i,mkChar(temp));
    R_Free(temp);
  } 
  setAttrib(tmp_value, R_NamesSymbol, tmp_names);
  SET_VECTOR_ELT(tmp_sexp,0,tmp_names);
  SET_VECTOR_ELT(tmp_sexp,1,tmp_value);
  SET_VECTOR_ELT(tmp_sexp,2,tmp_type);
  SET_VECTOR_ELT(return_value,1,tmp_sexp);
  UNPROTECT(4); 

  PROTECT(tmp_sexp= allocVector(VECSXP,my_data_set->ncols));  
  SET_VECTOR_ELT(return_value,2,tmp_sexp);
  PROTECT(tmp_names =  allocVector(STRSXP,my_data_set->ncols));
  for (i=0; i < my_data_set->ncols; i++){
     temp = R_Calloc(my_data_set->col_name_type_value[i].name.len+1,char);
     wcstombs(temp, my_data_set->col_name_type_value[i].name.value, my_data_set->col_name_type_value[i].name.len);
     SET_STRING_ELT(tmp_names,i,mkChar(temp));
     R_Free(temp);
  }
  setAttrib(tmp_sexp, R_NamesSymbol, tmp_names); 
  UNPROTECT(2);


  PROTECT(tmp_sexp = allocVector(VECSXP,3));
  PROTECT(tmp_names =  allocVector(STRSXP,my_data_set->ncols));
  PROTECT(tmp_value =  allocVector(INTSXP,my_data_set->ncols));
  PROTECT(tmp_size =  allocVector(INTSXP,my_data_set->ncols));
  for (i=0; i < my_data_set->ncols; i++){
    temp = R_Calloc(my_data_set->col_name_type_value[i].name.len+1,char);
    wcstombs(temp, my_data_set->col_name_type_value[i].name.value, my_data_set->col_name_type_value[i].name.len);
    SET_STRING_ELT(tmp_names,i,mkChar(temp));
    R_Free(temp);
    INTEGER(tmp_value)[i] = (int) my_data_set->col_name_type_value[i].type;
    INTEGER(tmp_size)[i] = (int) my_data_set->col_name_type_value[i].size;
  }
  SET_VECTOR_ELT(tmp_sexp,0,tmp_names);
  SET_VECTOR_ELT(tmp_sexp,1,tmp_value);
  SET_VECTOR_ELT(tmp_sexp,2,tmp_size);

  PROTECT(return_names = allocVector(STRSXP,3));
  SET_STRING_ELT(return_names,0,mkChar("Name"));
  SET_STRING_ELT(return_names,1,mkChar("ValueType"));
  SET_STRING_ELT(return_names,2,mkChar("Size"));
  setAttrib(tmp_sexp, R_NamesSymbol, return_names); 
  UNPROTECT(1);

  
  SET_VECTOR_ELT(return_value,3,tmp_sexp);
  UNPROTECT(4);
  

  PROTECT(return_names = allocVector(STRSXP,4));
  SET_STRING_ELT(return_names,0,mkChar("Name"));
  SET_STRING_ELT(return_names,1,mkChar("NVTList"));
  SET_STRING_ELT(return_names,2,mkChar("DataColumns"));
  SET_STRING_ELT(return_names,3,mkChar("DataColumnNTS"));
  setAttrib(return_value, R_NamesSymbol, return_names); 
  UNPROTECT(2);
  return return_value;

}








static SEXP generic_data_set_rows_R_List(generic_data_set *data_set, int col){

  SEXP return_value= R_NilValue;
  int i,j;
  char *temp;  

  j = col;

  switch(data_set->col_name_type_value[j].type){ 
  case 0:
     PROTECT(return_value = allocVector(INTSXP, data_set->nrows)); 
     for (i=0; i < data_set->nrows; i++){
       INTEGER_POINTER(return_value)[i] = (int32_t)((char *)data_set->Data[j])[i];
     }
     break;
  case 1:	
     PROTECT(return_value = allocVector(INTSXP, data_set->nrows));   
     for (i=0; i < data_set->nrows; i++){
       INTEGER_POINTER(return_value)[i] = (int32_t)((unsigned char *)data_set->Data[j])[i];
     }
     break;
  case 2:	
     PROTECT(return_value = allocVector(INTSXP, data_set->nrows));
     for (i=0; i < data_set->nrows; i++){
       INTEGER_POINTER(return_value)[i] = (int32_t)((short *)data_set->Data[j])[i];
     }
     break;
  case 3:	
     PROTECT(return_value = allocVector(INTSXP, data_set->nrows));  
     for (i=0; i < data_set->nrows; i++){
       INTEGER_POINTER(return_value)[i] = (int32_t)((unsigned short *)data_set->Data[j])[i];
     }
     break;  
  case 4:	
     PROTECT(return_value = allocVector(INTSXP, data_set->nrows));  
     for (i=0; i < data_set->nrows; i++){
       INTEGER_POINTER(return_value)[i] = (int32_t)((int32_t *)data_set->Data[j])[i];
     }
     break;
  case 5:	
     PROTECT(return_value = allocVector(INTSXP, data_set->nrows));
     for (i=0; i < data_set->nrows; i++){
       INTEGER_POINTER(return_value)[i] = (int32_t)((uint32_t *)data_set->Data[j])[i];
     }
   
     break;
  case 6:	
    PROTECT( return_value = allocVector(REALSXP, data_set->nrows));  
    for (i=0; i < data_set->nrows; i++){
       NUMERIC_POINTER(return_value)[i] = (double)((float *)data_set->Data[j])[i];
     }
     break;
/*  case 7:	
     PROTECT(return_value = allocVector(REALSXP, data_set->nrows));  
     for (i=0; i < data_set->nrows; i++){
       NUMERIC_POINTER(return_value)[i] = (double)((double *)data_set->Data[j])[i];
     }
     break; */
  case 7:	
     PROTECT(return_value = allocVector(STRSXP, data_set->nrows));
     for (i=0; i < data_set->nrows; i++){
       if ((int32_t)((ASTRING *)data_set->Data[j])[i].len > 0){
	 temp = (char *)((ASTRING *)data_set->Data[j])[i].value;
	 SET_STRING_ELT(return_value,i,mkChar(temp));
       }
     }
     break;
  case 8:	
     PROTECT(return_value = allocVector(STRSXP, data_set->nrows));
     for (i=0; i < data_set->nrows; i++){
       temp = R_Calloc(((AWSTRING *)data_set->Data[j])[i].len+1,char);
       wcstombs(temp, ((AWSTRING *)data_set->Data[j])[i].value,((AWSTRING *)data_set->Data[j])[i].len);
       SET_STRING_ELT(return_value,i,mkChar(temp));
       R_Free(temp);
     }
     break;
  }
  UNPROTECT(1);
  return return_value;
}




SEXP Read_Generic_R_List(SEXP filename, SEXP reducedoutput){

  int i,j,k;

  int shorten_NVT = asInteger(reducedoutput);
  
  SEXP return_value = R_NilValue;
  SEXP return_names;
  SEXP temp_sxp = R_NilValue,temp_sxp2 = R_NilValue,temp_names = R_NilValue,temp_names2 = R_NilValue;	
  FILE *infile;

  char *temp;

  generic_file_header my_header;
  generic_data_header my_data_header;
  generic_data_group my_data_group;

  generic_data_set my_data_set;

  const char *cur_file_name = CHAR(STRING_ELT(filename,0));

  /* Pass through all the header information */
  
  if ((infile = fopen(cur_file_name, "rb")) == NULL)
    {
      error("Unable to open the file %s\n",cur_file_name);
      return 0;
    }
  

  /* Read the two header sections first */
  read_generic_file_header(&my_header, infile);
  read_generic_data_header(&my_data_header, infile);
  	
  PROTECT(return_value = allocVector(VECSXP,3));

  /* File Header is First Element of Return List */
	
  SET_VECTOR_ELT(return_value,0,file_header_R_List(&my_header));

  /* Data Header is Second Element of Return List */
  if (shorten_NVT){
    SET_VECTOR_ELT(return_value,1,data_header_R_List(&my_data_header));
  } else {
    SET_VECTOR_ELT(return_value,1,data_header_R_List_full(&my_data_header));
  }

    
  /* Data Groups are it Third Element of Return List */	
  /* Now Read Data groups */	
  
  PROTECT(temp_sxp = allocVector(VECSXP,my_header.n_data_groups));	
  SET_VECTOR_ELT(return_value,2,temp_sxp);
  UNPROTECT(1);
  PROTECT(temp_names = allocVector(STRSXP,my_header.n_data_groups));	
  for (k =0; k < my_header.n_data_groups; k++){
    read_generic_data_group(&my_data_group,infile);
    SET_VECTOR_ELT(temp_sxp,k,data_group_R_list(&my_data_group));
             
    temp = R_Calloc(my_data_group.data_group_name.len+1,char);
    wcstombs(temp, my_data_group.data_group_name.value, my_data_group.data_group_name.len);
    SET_STRING_ELT(temp_names,k,mkChar(temp));  
    R_Free(temp);
    
    PROTECT(temp_names2 = allocVector(STRSXP,my_data_group.n_data_sets));	
    for (j=0; j < my_data_group.n_data_sets; j++){
      read_generic_data_set(&my_data_set,infile);

      if (shorten_NVT){
	temp_sxp2 = generic_data_set_R_List(&my_data_set);
      } else {
	temp_sxp2 =  generic_data_set_R_List_full(&my_data_set);
      }
      
      SET_VECTOR_ELT(VECTOR_ELT(VECTOR_ELT(temp_sxp,k),1),j,temp_sxp2);

      temp = R_Calloc(my_data_set.data_set_name.len+1,char);
      wcstombs(temp, my_data_set.data_set_name.value, my_data_set.data_set_name.len);
      SET_STRING_ELT(temp_names2,j,mkChar(temp));  
      R_Free(temp);	

      read_generic_data_set_rows(&my_data_set,infile); 

      for (i =0; i < my_data_set.ncols; i++){
	SET_VECTOR_ELT(VECTOR_ELT(temp_sxp2,2),i,generic_data_set_rows_R_List(&my_data_set, i));
      }
    
      fseek(infile, my_data_set.file_pos_last, SEEK_SET);
      Free_generic_data_set(&my_data_set);
    }
    fseek(infile, my_data_group.file_position_nextgroup, SEEK_SET);
    setAttrib(VECTOR_ELT(VECTOR_ELT(temp_sxp,k),1), R_NamesSymbol, temp_names2); 
    UNPROTECT(1);	

    Free_generic_data_group(&my_data_group);
  }
  Free_generic_data_header(&my_data_header);
  setAttrib(temp_sxp, R_NamesSymbol, temp_names); 
  UNPROTECT(1);

  PROTECT(return_names = allocVector(STRSXP,3));
  SET_STRING_ELT(return_names,0,mkChar("FileHeader"));
  SET_STRING_ELT(return_names,1,mkChar("DataHeader"));
  SET_STRING_ELT(return_names,2,mkChar("DataGroup"));
  setAttrib(return_value, R_NamesSymbol, return_names); 
  UNPROTECT(2);
  fclose(infile);
  return return_value;
}

